#pragma once

#include <cstdint>
#include <string>

#include "i2cpp/device.hpp"

namespace picar_hw
{

/// One PWM channel on the HAT's MCU, driven over I2C.
///
/// Register addresses and the timer layout are in hat_registers.hpp. Channels
/// sharing a timer share its prescaler and period, so set_frequency() on one
/// channel changes every sibling on the same timer.
class Hat : public i2cpp::Device
{
public:
    /// @param bus     I2C bus number; opens /dev/i2c-<bus>
    /// @param address 7-bit address of the MCU
    /// @param channel PWM channel P0-P13
    Hat(int bus, uint_fast8_t address, uint8_t channel);

    /// Same, against a device file. Tests point this at a temp file to capture
    /// the register writes.
    Hat(std::string filename, uint_fast8_t address, uint8_t channel);

    /// Set the PWM frequency of this channel's timer, in Hz.
    void set_frequency(uint16_t frequency);

    /// Write the raw compare value for this channel.
    void set_pulse_width(uint16_t pulse_width);

    /// Set the duty cycle as a percentage of the current period, 0-100.
    void set_duty_cycle(float duty_cycle);

private:
    void set_prescaler(uint16_t prescaler);
    void set_period(uint16_t period);

    uint8_t channel_;
    uint8_t timer_;
    uint16_t period_ = 0;
};

}  // namespace picar_hw
