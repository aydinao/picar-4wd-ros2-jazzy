#pragma once

#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_component_interface_params.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "rclcpp_lifecycle/state.hpp"

namespace picar_4wd_hardware
{

// ---------------------------------------------------------------------------
// SystemInterface implementation for the PiCar-4WD.
//
// Loaded by controller_manager via pluginlib -- there is no main(). Call order:
//
//   on_init       parse the URDF (params.hardware_info). NO hardware access.
//   on_configure  acquire hardware: pulse GPIO21, open the bus, set frequency.
//   on_activate   safe starting state, then commands are allowed.
//   read/write    every control cycle, forever. REAL-TIME PATH.
//   on_deactivate stop the motors.
//
// API NOTE: this targets ros2_control 4.45 (what RoboStack ships), which is
// NEWER than the API used by ros2_control_demos on the `jazzy` branch. Two
// things differ from that reference:
//   * on_init takes HardwareComponentInterfaceParams, not HardwareInfo.
//   * export_state_interfaces()/export_command_interfaces() are deprecated.
//     The base class builds the interfaces from the URDF automatically, and
//     you reach them by NAME with set_state()/get_command(). There are no
//     member vectors to manage and no pointers handed out.
// The installed header is the source of truth:
//   .pixi/envs/default/include/hardware_interface/hardware_interface/
// ---------------------------------------------------------------------------
class PiCarSystemHardware : public hardware_interface::SystemInterface
{
public:
    hardware_interface::CallbackReturn on_init(
        const hardware_interface::HardwareComponentInterfaceParams & params) override;

    hardware_interface::CallbackReturn on_configure(
        const rclcpp_lifecycle::State & previous_state) override;

    hardware_interface::CallbackReturn on_activate(
        const rclcpp_lifecycle::State & previous_state) override;

    hardware_interface::CallbackReturn on_deactivate(
        const rclcpp_lifecycle::State & previous_state) override;

    hardware_interface::return_type read(
        const rclcpp::Time & time, const rclcpp::Duration & period) override;

    hardware_interface::return_type write(
        const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
    // Hardware lives here. Types deliberately left for you: how many HAT
    // objects there are, who owns them, and whether the two encoders are one
    // object or two is exactly the design question this port forces.
};

}  // namespace picar_4wd_hardware
