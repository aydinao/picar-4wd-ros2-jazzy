#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "picar_hw/hat.hpp"
#include "picar_hw/motor.hpp"

#include "hardware_interface/handle.hpp"
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
/// How one wheel is wired, read from its <joint> block in the URDF.
struct WheelConfig
{
    std::string joint_name;
    uint8_t pwm_channel;   ///< HAT PWM channel, P0-P13
    uint8_t dir_gpio;      ///< BCM number of the direction line
    bool reversed;         ///< true for the mirrored rear wheels
    bool on_left;          ///< which side's encoder measures this wheel
};

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
    /// Drive every motor to zero through the HAT.
    ///
    /// Used by on_activate and on_deactivate. Both need the hardware itself
    /// stopped, which zeroing a command interface does not achieve: write() is
    /// not called before activation or after deactivation, so nothing would
    /// carry the value out to the chip.
    void stop_all_motors();

    // Configuration parsed from the URDF in on_init(). Hardware handles are
    // not held yet: the HAT and Motor objects are constructed in on_configure()
    // once they exist.
    uint8_t i2c_bus_;
    uint_fast8_t i2c_address_;
    uint16_t pwm_frequency_hz_;
    std::string gpio_chip_;
    uint8_t left_encoder_gpio_;
    uint8_t right_encoder_gpio_;
    uint8_t encoder_slots_per_rev_;
    uint8_t reset_gpio_;

    /// Wheel velocity that corresponds to full power, in rad/s.
    ///
    /// CALIBRATION CONSTANT, not physics: it is the scale factor between the
    /// controller's rad/s and the motor's -100..100. Measured by driving the
    /// robot and timing it over a known distance; see the URDF param.
    double max_wheel_speed_rad_s_ = 0.0;

    /// One entry per <joint>, in the order the URDF declares them.
    std::vector<WheelConfig> wheels_;

    // Hardware, created in on_configure(). Held by pointer because Motor owns
    // a GPIO request and holds a Hat reference, so it is neither copyable nor
    // movable and cannot live directly in a vector.
    //
    // DECLARATION ORDER IS LOAD-BEARING: members are destroyed in reverse
    // order, so motors_ is torn down before hats_. A Motor holds a Hat& and
    // touches it in its destructor.
    std::vector<std::unique_ptr<picar_hw::Hat>> hats_;
    std::vector<std::unique_ptr<picar_hw::Motor>> motors_;

    /// Command interface handles, in the same URDF order as wheels_ and
    /// motors_, resolved once in on_configure().
    ///
    /// write() must not use the by-name get_command(): that overload looks the
    /// interface up by string and waits for the value to settle, and the
    /// installed header documents it as NOT real-time safe. Resolving the
    /// handle once and reading through it every cycle is the real-time path.
    std::vector<hardware_interface::CommandInterface::SharedPtr> velocity_commands_;
};

}  // namespace picar_ros
