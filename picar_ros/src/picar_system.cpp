#include "picar_ros/picar_system.hpp"

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "pluginlib/class_list_macros.hpp"
#include "picar_hw/reset.hpp"
#include "rclcpp/rclcpp.hpp"

#include <stdexcept>
#include <string>

namespace picar_ros
{

hardware_interface::CallbackReturn PiCarSystemHardware::on_init(
    const hardware_interface::HardwareComponentInterfaceParams & params)
{
    if (SystemInterface::on_init(params) != hardware_interface::CallbackReturn::SUCCESS) {
        return hardware_interface::CallbackReturn::ERROR;
    }
    // Hardware-wide configuration. No hardware is touched here -- on_init runs
    // before the component is allowed to claim anything.
    //
    // .at() rather than operator[]: operator[] inserts an empty string for a
    // missing key, so a typo in the xacro would surface as a confusing parse
    // failure instead of a missing-key error.
    //

    i2c_bus_ = hardware_interface::stoui8(get_hardware_info().hardware_parameters.at("i2c_bus"));

    // The address is the only param written in hex, so the only one needing
    // base 0 (a C integer literal: 0x14 and 20 both mean 20). None of
    // hardware_interface's stoui* helpers can do that -- they are base 10.
    const std::string & addr_str = get_hardware_info().hardware_parameters.at("i2c_address");
    std::size_t pos = 0;
    const unsigned long addr = std::stoul(addr_str, &pos, 0);
    if (pos != addr_str.length()) {
        throw std::invalid_argument("i2c_address_ has trailing characters: " + addr_str);
    }
    if (addr > 0x7F) {
        throw std::out_of_range("i2c_address_ is not a 7-bit address: " + addr_str);
    }
    i2c_address_ = static_cast<uint_fast8_t>(addr);

    pwm_frequency_hz_ = hardware_interface::stoui16(get_hardware_info().hardware_parameters.at("pwm_frequency_hz"));
    gpio_chip_ = get_hardware_info().hardware_parameters.at("gpio_chip");

    left_encoder_gpio_ = hardware_interface::stoui8(get_hardware_info().hardware_parameters.at("left_encoder_gpio"));
    right_encoder_gpio_ = hardware_interface::stoui8(get_hardware_info().hardware_parameters.at("right_encoder_gpio"));
    encoder_slots_per_rev_ = hardware_interface::stoui8(get_hardware_info().hardware_parameters.at("encoder_slots_per_rev"));
    reset_gpio_ = hardware_interface::stoui8(get_hardware_info().hardware_parameters.at("reset_gpio"));

    // Per-wheel wiring. Kept in URDF order so wheels_[i] and
    // get_hardware_info().joints[i] always refer to the same wheel.
    wheels_.clear();
    wheels_.reserve(get_hardware_info().joints.size());
    for (const auto & joint : get_hardware_info().joints) {
        const auto & p = joint.parameters;

        const std::string & side = p.at("encoder_side");
        if (side != "left" && side != "right") {
            throw std::invalid_argument(
                "joint '" + joint.name + "' has encoder_side '" + side +
                "', expected \"left\" or \"right\"");
        }

        wheels_.push_back(WheelConfig{
            joint.name,
            hardware_interface::stoui8(p.at("pwm_channel")),
            hardware_interface::stoui8(p.at("dir_gpio")),
            hardware_interface::parse_bool(p.at("reversed")),
            side == "left",
        });
    }

    RCLCPP_INFO(get_logger(), "Configured %zu wheels on i2c-%u at 0x%02X",
                wheels_.size(), static_cast<unsigned>(i2c_bus_),
                static_cast<unsigned>(i2c_address_));

    return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn PiCarSystemHardware::on_configure(
    const rclcpp_lifecycle::State & /*previous_state*/)
{
    // Release the MCU from reset before anything tries the bus.
    if (!picar_hw::pulse_reset(gpio_chip_, reset_gpio_)) {
        RCLCPP_ERROR(get_logger(), "could not pulse reset on %s line %u",
                     gpio_chip_.c_str(), static_cast<unsigned>(reset_gpio_));
        return hardware_interface::CallbackReturn::ERROR;
    }

    // One Hat per wheel. The HAT is a single chip, but Hat models one PWM
    // channel, so four objects address four channels on the same device --
    // i2cpp shares one file descriptor between them.
    //
    // This is only safe because every wheel runs at the SAME frequency:
    // channels 12/13 share timer 3 and 8/9 share timer 2, and each Hat caches
    // its own period. Give two wheels on one timer different frequencies and
    // the second silently changes the first. See picar_hw/README.md.
    hats_.clear();
    motors_.clear();
    hats_.reserve(wheels_.size());
    motors_.reserve(wheels_.size());

    for (const auto & w : wheels_) {
        auto hat = std::make_unique<picar_hw::Hat>(
            static_cast<int>(i2c_bus_), i2c_address_, w.pwm_channel);
        hat->set_frequency(pwm_frequency_hz_);

        auto motor = std::make_unique<picar_hw::Motor>(*hat, w.dir_gpio, w.reversed);
        if (!motor->ok()) {
            RCLCPP_ERROR(get_logger(), "joint '%s': could not claim GPIO %u",
                         w.joint_name.c_str(), static_cast<unsigned>(w.dir_gpio));
            hats_.clear();
            motors_.clear();
            return hardware_interface::CallbackReturn::ERROR;
        }

        hats_.push_back(std::move(hat));
        motors_.push_back(std::move(motor));
    }
    for (const auto & [name, descr] : joint_state_interfaces_)
    {
        set_state(name, 0.0);
    }
    for (const auto & [name, descr] : joint_command_interfaces_)
    {
        set_command(name, 0.0);
    }
    RCLCPP_INFO(get_logger(), "Configured %zu motors at %u Hz", motors_.size(),
                static_cast<unsigned>(pwm_frequency_hz_));
    return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn PiCarSystemHardware::on_activate(
    const rclcpp_lifecycle::State & /*previous_state*/)
{
    // TODO: drive the motors to zero before commands are accepted.

    // Command must match state on activation, or the robot lurches to
    // whatever stale value was sitting in the command interface.
  for (const auto & [name, descr] : joint_command_interfaces_)
    {
        set_command(name, get_state(name));
    }

    RCLCPP_INFO(get_logger(), "Successfully activated!");
    return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn PiCarSystemHardware::on_deactivate(
    const rclcpp_lifecycle::State & /*previous_state*/)
{
    // TODO: stop the motors by writing to the hardware directly.
    //
    // Zeroing a command interface is not sufficient: write() is no longer
    // called once the component is deactivated, so nothing would carry the
    // value out to the HAT. This is the documented "wheels spin forever"
    // limitation in the README.
    return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::return_type PiCarSystemHardware::read(
    const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
    // TODO: convert encoder counts to wheel velocity and publish it with
    // set_state("<joint>/velocity", value).
    //
    // The robot has two encoders and four wheels -- one per side, single
    // channel, 20 slots per revolution (recovered from SunFounder's
    // picar_4wd/speed.py). Per-wheel velocity is therefore not measurable, so
    // what this reports for each of the four joints is a modelling choice.
    //
    // Real-time path: no allocation, no blocking, no logging.
    return hardware_interface::return_type::OK;
}

hardware_interface::return_type PiCarSystemHardware::write(
    const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
    // TODO: read commands with get_command("<joint>/velocity"), convert
    // rad/s to a duty cycle and a direction, and drive the HAT.
    //
    // Real-time path, and it performs I2C: four register writes per cycle at
    // the controller_manager update rate. That cost needs measuring against
    // the control period rather than assuming it fits.
    return hardware_interface::return_type::OK;
}

}  // namespace picar_ros

PLUGINLIB_EXPORT_CLASS(
    picar_ros::PiCarSystemHardware, hardware_interface::SystemInterface)
