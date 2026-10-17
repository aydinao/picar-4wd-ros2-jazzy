#include "picar_ros/picar_system.hpp"

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "pluginlib/class_list_macros.hpp"
#include "picar_hw/reset.hpp"
#include "rclcpp/rclcpp.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace picar_ros
{

namespace
{

/// Commanded wheel velocity (rad/s) -> the motor's -100..100 power scale.
///
/// Linear, with a single constant. There is no physics to appeal to here: duty
/// cycle sets the average voltage across a brushed DC motor, and the speed that
/// results depends on load, gearbox and battery charge. max_speed_rad_s is
/// therefore a CALIBRATION value -- measured, never derived -- and this mapping
/// is a first approximation to a curve that is not actually straight, least of
/// all near zero where the motor does not turn at all.
///
/// Motor::set_power already owns the two things below this: the sign selects
/// the direction pin, and non-zero magnitudes are remapped into the 50-100%
/// band where the motor overcomes stiction.
int8_t velocity_to_power(double velocity_rad_s, double max_speed_rad_s)
{
    // A NaN command must not reach the hardware. It would compare false against
    // both clamps and cast to garbage.
    if (!std::isfinite(velocity_rad_s)) {
        return 0;
    }

    double duty = 100.0 * velocity_rad_s / max_speed_rad_s;

    if (duty >  100.0) duty =  100.0;
    if (duty < -100.0) duty = -100.0;

    return static_cast<int8_t>(std::lround(duty));
}

}  // namespace

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

    // Divisor in velocity_to_power, so it must be positive and finite. Checked
    // here rather than in the control loop, where there is nothing useful to do
    // about it.
    max_wheel_speed_rad_s_ = hardware_interface::stod(
        get_hardware_info().hardware_parameters.at("max_wheel_speed_rad_s"));
    if (!std::isfinite(max_wheel_speed_rad_s_) || max_wheel_speed_rad_s_ <= 0.0) {
        throw std::invalid_argument(
            "max_wheel_speed_rad_s must be finite and positive, got: " +
            get_hardware_info().hardware_parameters.at("max_wheel_speed_rad_s"));
    }
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

    // Resolve the command interface handles once, here, so write() never has to
    // look one up by name. The by-name accessors build a string key and wait on
    // the value; the installed hardware_component_interface.hpp documents them
    // as not real-time safe. Same URDF order as wheels_ and motors_.
    // The key format is prefix + "/" + interface, built in InterfaceDescription's
    // constructor -- so "left_front_wheel_joint/velocity". Checked against the
    // map first: this code only ever runs on the robot, and a mismatch here
    // would otherwise surface as an opaque throw from inside the base class.
    velocity_commands_.clear();
    velocity_commands_.reserve(wheels_.size());
    for (const auto & w : wheels_) {
        const std::string interface_name = w.joint_name + "/velocity";
        if (joint_command_interfaces_.find(interface_name) == joint_command_interfaces_.end()) {
            RCLCPP_ERROR(get_logger(),
                         "no command interface '%s' -- the URDF joint name and the "
                         "ros2_control joint name disagree",
                         interface_name.c_str());
            hats_.clear();
            motors_.clear();
            velocity_commands_.clear();
            return hardware_interface::CallbackReturn::ERROR;
        }
        velocity_commands_.push_back(get_command_interface_handle(interface_name));
    }
    RCLCPP_INFO(get_logger(), "Configured %zu motors at %u Hz", motors_.size(),
                static_cast<unsigned>(pwm_frequency_hz_));
    return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn PiCarSystemHardware::on_activate(
    const rclcpp_lifecycle::State & /*previous_state*/)
{
    // Nothing may be turning at the instant commands start being honoured. The
    // HAT keeps its last pulse-width across a deactivate/activate cycle, so
    // without this a re-activated robot resumes at whatever it was doing.
    stop_all_motors();

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
    // Stop the hardware, not the command interface. write() is not called again
    // after deactivation, so a zeroed command would sit in memory while the
    // wheels kept turning -- the "wheels spin forever" limitation the README
    // has carried since June.
    //
    // Note what this still does not cover: SIGKILL, a lost battery, or the
    // process being OOM-killed all bypass every lifecycle callback. Only a
    // watchdog on the HAT itself would close that gap.
    stop_all_motors();

    RCLCPP_INFO(get_logger(), "Deactivated: all %zu motors stopped", motors_.size());
    return hardware_interface::CallbackReturn::SUCCESS;
}

void PiCarSystemHardware::stop_all_motors()
{
    for (auto & motor : motors_) {
        if (motor) {
            motor->set_power(0);
        }
    }
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
    // REAL-TIME PATH. No allocation, no logging, no locks. The one unavoidable
    // cost is the I2C traffic: Motor::set_power writes one three-byte pulse
    // width register per wheel, so four register writes per cycle at 20 Hz.
    // That is still an assumption -- it has not been timed on the Pi against
    // the 50 ms period.
    for (std::size_t i = 0; i < motors_.size(); ++i) {
        double command_rad_s = 0.0;

        // Handle overload with wait_until_get = false: never block the control
        // loop waiting for a value to settle. A false return means the command
        // was not readable this cycle, which is not an error -- the motor keeps
        // its previous setting until the next one arrives.
        if (!get_command(velocity_commands_[i], command_rad_s, false)) {
            continue;
        }

        motors_[i]->set_power(
            velocity_to_power(command_rad_s, max_wheel_speed_rad_s_));
    }

    return hardware_interface::return_type::OK;
}

}  // namespace picar_ros

PLUGINLIB_EXPORT_CLASS(
    picar_ros::PiCarSystemHardware, hardware_interface::SystemInterface)
