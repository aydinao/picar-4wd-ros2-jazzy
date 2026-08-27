#include "picar_4wd_hardware/picar_system.hpp"

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "pluginlib/class_list_macros.hpp"
#include "rclcpp/rclcpp.hpp"

namespace picar_4wd_hardware
{

hardware_interface::CallbackReturn PiCarSystemHardware::on_init(
    const hardware_interface::HardwareInfo & info)
{
    if (SystemInterface::on_init(info) != hardware_interface::CallbackReturn::SUCCESS) {
        return hardware_interface::CallbackReturn::ERROR;
    }
    // TODO(you): read params from info_.hardware_parameters and info_.joints.
    // NO hardware access here.
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
    // TODO(you): zero every motor, zero the state/command vectors.
    return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn PiCarSystemHardware::on_deactivate(
    const rclcpp_lifecycle::State & /*previous_state*/)
{
    // TODO(you): STOP THE MOTORS. This is the fix for "wheels spin forever".
    return hardware_interface::CallbackReturn::SUCCESS;
}

std::vector<hardware_interface::StateInterface>
PiCarSystemHardware::export_state_interfaces()
{
    std::vector<hardware_interface::StateInterface> state_interfaces;
    // TODO(you): one entry per (joint, interface) pair you claim to provide.
    // You have TWO encoders and FOUR wheels -- what you export here is a
    // design decision, not a transcription.
    return state_interfaces;
}

std::vector<hardware_interface::CommandInterface>
PiCarSystemHardware::export_command_interfaces()
{
    std::vector<hardware_interface::CommandInterface> command_interfaces;
    // TODO(you): velocity command per driven joint.
    return command_interfaces;
}

hardware_interface::return_type PiCarSystemHardware::read(
    const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
    // TODO(you): encoder counts -> wheel velocity -> hw_states_*.
    // REAL-TIME PATH: no allocation, no blocking, no logging.
    return hardware_interface::return_type::OK;
}

hardware_interface::return_type PiCarSystemHardware::write(
    const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
    // TODO(you): hw_commands_velocity_ -> duty cycle + direction -> HAT.
    // REAL-TIME PATH: this does I2C. Think about what that means.
    return hardware_interface::return_type::OK;
}

}  // namespace picar_4wd_hardware

PLUGINLIB_EXPORT_CLASS(
    picar_4wd_hardware::PiCarSystemHardware, hardware_interface::SystemInterface)
