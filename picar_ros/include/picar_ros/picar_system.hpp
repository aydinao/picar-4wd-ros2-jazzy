#pragma once

#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_component_interface_params.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "rclcpp_lifecycle/state.hpp"

namespace picar_ros
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
// API NOTE: targets hardware_interface 4.45.1, which matches
// ros2_control_demos example_2 on the `jazzy` branch. Interfaces are NOT
// exported by hand -- the base class builds them from the URDF and you reach
// them by name with set_state()/get_command(). The installed header is the
// source of truth:
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

}  // namespace picar_ros
