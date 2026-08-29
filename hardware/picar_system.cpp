#include "picar_4wd_hardware/picar_system.hpp"

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "pluginlib/class_list_macros.hpp"
#include "rclcpp/rclcpp.hpp"

namespace picar_4wd_hardware
{

hardware_interface::CallbackReturn PiCarSystemHardware::on_init(
    const hardware_interface::HardwareComponentInterfaceParams & params)
{
    if (SystemInterface::on_init(params) != hardware_interface::CallbackReturn::SUCCESS) {
        return hardware_interface::CallbackReturn::ERROR;
    }
    // TODO(you): read params from info_.hardware_parameters and info_.joints.
    // The base call above has already populated info_ and created the state and
    // command interfaces declared in the URDF. NO hardware access here.
    return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn PiCarSystemHardware::on_configure(
    const rclcpp_lifecycle::State & /*previous_state*/)
{
    // TODO(you): pulse GPIO21, open the I2C bus, set PWM frequency,
    // claim the direction and encoder GPIO lines.
    return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn PiCarSystemHardware::on_activate(
    const rclcpp_lifecycle::State & /*previous_state*/)
{
    // TODO(you): zero every motor and every state/command interface.
    return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn PiCarSystemHardware::on_deactivate(
    const rclcpp_lifecycle::State & /*previous_state*/)
{
    // TODO(you): STOP THE MOTORS. This is the fix for "wheels spin forever".
    return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::return_type PiCarSystemHardware::read(
    const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
    // TODO(you): encoder counts -> wheel velocity, then publish it with
    //   set_state("<joint>/velocity", value);
    // You have TWO encoders and FOUR wheels. What you set here is a design
    // decision, not a transcription.
    // REAL-TIME PATH: no allocation, no blocking, no logging.
    return hardware_interface::return_type::OK;
}

hardware_interface::return_type PiCarSystemHardware::write(
    const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
    // TODO(you): read the commands with
    //   get_command("<joint>/velocity");
    // convert rad/s -> duty cycle + direction, then drive the HAT.
    // REAL-TIME PATH: this does I2C. Think about what that means.
    return hardware_interface::return_type::OK;
}

}  // namespace picar_4wd_hardware

PLUGINLIB_EXPORT_CLASS(
    picar_4wd_hardware::PiCarSystemHardware, hardware_interface::SystemInterface)
