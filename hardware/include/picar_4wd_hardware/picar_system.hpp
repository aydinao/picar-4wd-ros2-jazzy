#pragma once

#include <memory>
#include <vector>

#include "hardware_interface/handle.hpp"
#include "hardware_interface/hardware_info.hpp"
#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "rclcpp_lifecycle/state.hpp"

namespace picar_4wd_hardware
{

// ---------------------------------------------------------------------------
// SystemInterface implementation for the PiCar-4WD.
//
// Loaded by controller_manager via pluginlib -- there is no main(). The
// lifecycle methods below are called by ros2_control, in this order:
//
//   on_init       parse the URDF. NO hardware access.
//   on_configure  acquire hardware: pulse GPIO21, open the bus, set frequency.
//   on_activate   safe starting state, then commands are allowed.
//   read/write    called every control cycle, forever. REAL-TIME PATH.
//   on_deactivate stop the motors.
//
// Signatures should be cross-checked against the installed header:
//   $CONDA_PREFIX/include/hardware_interface/system_interface.hpp
// ---------------------------------------------------------------------------
class PiCarSystemHardware : public hardware_interface::SystemInterface
{
public:
    hardware_interface::CallbackReturn on_init(
        const hardware_interface::HardwareInfo & info) override;

    hardware_interface::CallbackReturn on_configure(
        const rclcpp_lifecycle::State & previous_state) override;

    hardware_interface::CallbackReturn on_activate(
        const rclcpp_lifecycle::State & previous_state) override;

    hardware_interface::CallbackReturn on_deactivate(
        const rclcpp_lifecycle::State & previous_state) override;

    std::vector<hardware_interface::StateInterface> export_state_interfaces() override;

    std::vector<hardware_interface::CommandInterface> export_command_interfaces() override;

    hardware_interface::return_type read(
        const rclcpp::Time & time, const rclcpp::Duration & period) override;

    hardware_interface::return_type write(
        const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
    // The vectors backing the exported interfaces must outlive them and must
    // not reallocate once exported -- export_*_interfaces() hands out pointers
    // into this storage.
    std::vector<double> hw_commands_velocity_;
    std::vector<double> hw_states_position_;
    std::vector<double> hw_states_velocity_;

    // Hardware lives here. Types deliberately left for you to decide -- how
    // many HAT objects, who owns them, and whether encoders are one object or
    // two is exactly the design question this port forces.
};

}  // namespace picar_4wd_hardware
