#include "picar_hw/hat.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace picar_hw
{

Hat::Hat(I2cBus & bus, std::uint32_t frequency_hz)
: bus_(bus)
{
  for (std::uint8_t timer = 0; timer < hat::kTimerCount; ++timer) {
    set_frequency(timer, frequency_hz);
  }
}

// RM0008, general-purpose timers: the counter ticks at kClockHz / (PSC + 1)
// and wraps after ARR + 1 counts, so one PWM period is (PSC + 1)(ARR + 1)
// clock ticks. Between the two, take the smallest prescaler that lets the
// period fit: that leaves ARR as large as possible and so gives the finest
// duty steps. It is the same choice the Linux STM32 PWM driver makes
// (stm32_pwm_round_waveform_tohw in drivers/pwm/pwm-stm32.c).
void Hat::set_frequency(std::uint8_t timer, std::uint32_t frequency_hz)
{
  if (timer >= hat::kTimerCount) {
    throw std::out_of_range("Hat: no timer " + std::to_string(timer));
  }
  if (frequency_hz < kMinFrequencyHz || frequency_hz > kMaxFrequencyHz) {
    throw std::invalid_argument(
            "Hat: frequency " + std::to_string(frequency_hz) + " Hz is outside " +
            std::to_string(kMinFrequencyHz) + "-" + std::to_string(kMaxFrequencyHz) + " Hz");
  }

  const std::uint32_t ticks = hat::kClockHz / frequency_hz;
  const std::uint32_t psc = ticks / (hat::kRegisterMax + 1);

  // Capped one below the register maximum, so that CCR = ARR + 1 (100% duty)
  // still fits in 16 bits.
  const std::uint32_t period_counts = std::min(ticks / (psc + 1), hat::kRegisterMax);

  write_register(
    static_cast<std::uint8_t>(hat::kRegPrescalerBase + timer),
    static_cast<std::uint16_t>(psc));
  write_register(
    static_cast<std::uint8_t>(hat::kRegAutoReloadBase + timer),
    static_cast<std::uint16_t>(period_counts - 1));
  period_counts_[timer] = period_counts;
}

void Hat::set_duty_cycle(std::uint8_t channel, float percent)
{
  const std::uint8_t timer = hat::timer_for_channel(channel);
  if (timer >= hat::kTimerCount) {
    throw std::out_of_range("Hat: no channel " + std::to_string(channel));
  }

  percent = std::clamp(percent, 0.0f, 100.0f);

  // The output is high while the counter is below CCR (PWM mode 1), so
  // CCR = period_counts holds it high for the whole period.
  const auto compare = static_cast<std::uint16_t>(
    static_cast<double>(percent) / 100.0 * period_counts_[timer]);

  write_register(static_cast<std::uint8_t>(hat::kRegChannelBase + channel), compare);
}

void Hat::write_register(std::uint8_t reg, std::uint16_t value)
{
  const std::uint8_t frame[hat::kRegisterWriteBytes] = {
    reg,
    static_cast<std::uint8_t>(value >> 8),
    static_cast<std::uint8_t>(value & 0xFF),
  };
  bus_.write(frame, sizeof(frame));
}

}  // namespace picar_hw
