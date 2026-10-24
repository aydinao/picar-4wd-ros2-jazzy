#pragma once

#include <array>
#include <cstdint>

#include "picar_hw/hat_registers.hpp"
#include "picar_hw/i2c_bus.hpp"

namespace picar_hw
{

/// The HAT's MCU: four timers, four PWM channels each, behind one I2C address.
///
/// Frequency belongs to a timer and duty to a channel, so the two are set
/// separately and a wheel can never change its sibling's frequency by
/// accident. Register addresses and the timer layout are in hat_registers.hpp.
class Hat
{
public:
  /// Accepted PWM frequencies. The motor drivers (TC1508S) state no limit;
  /// 20 kHz is a policy ceiling, already above audible whine.
  static constexpr std::uint32_t kMinFrequencyHz = 1;
  static constexpr std::uint32_t kMaxFrequencyHz = 20000;

  /// Sets every timer to `frequency_hz`, so each channel has a period before
  /// its first duty write.
  /// @param bus must outlive this Hat
  /// @throws std::invalid_argument if the frequency is out of range
  Hat(I2cBus & bus, std::uint32_t frequency_hz);

  // A copy would hold its own periods, which go stale when the other copy
  // changes a timer: the bug this class exists to remove.
  Hat(const Hat &) = delete;
  Hat & operator=(const Hat &) = delete;
  Hat(Hat &&) = delete;
  Hat & operator=(Hat &&) = delete;

  /// Set one timer's PWM frequency, shared by its four channels.
  /// @throws std::out_of_range for a timer >= hat::kTimerCount
  /// @throws std::invalid_argument if the frequency is out of range
  void set_frequency(std::uint8_t timer, std::uint32_t frequency_hz);

  /// Set one channel's duty cycle as a percentage of its timer's period,
  /// clamped to 0-100. 100 holds the output high for the whole period.
  /// @throws std::out_of_range for a channel on no timer
  void set_duty_cycle(std::uint8_t channel, float percent);

private:
  /// One [reg, value_high, value_low] write.
  void write_register(std::uint8_t reg, std::uint16_t value);

  I2cBus & bus_;

  /// Counts per PWM period for each timer: ARR + 1, not the ARR register.
  std::array<std::uint32_t, hat::kTimerCount> period_counts_{};
};

}  // namespace picar_hw
