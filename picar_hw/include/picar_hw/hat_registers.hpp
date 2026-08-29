#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------
// Facts about the HAT's onboard MCU.
//
// These describe the CHIP and never change from robot to robot, which is why
// they live in a header rather than in the URDF. Anything that describes how
// THIS robot is wired (which PWM channel drives which wheel, which BCM pin is
// its direction line, encoder pins, wheel radius) belongs in the URDF and is
// read in on_init() -- not here.
//
// Source: reverse-engineered from SunFounder's `picar_4wd` Python library;
// there is no public schematic. The register names and the 72 MHz clock match
// STM32 general-purpose timer conventions (PSC = prescaler, ARR = auto-reload),
// so ST's RM0008 timer chapter documents the far side of the bus.
// ---------------------------------------------------------------------------

namespace picar_hw::hat
{

/// Per-channel pulse width (compare) register base. Channel N is kRegChannelBase + N.
inline constexpr std::uint8_t kRegChannelBase = 0x20;

/// Frequency register base (unused by the current driver; frequency is set via PSC/ARR).
inline constexpr std::uint8_t kRegFrequencyBase = 0x30;

/// Prescaler register base. Timer T is kRegPrescalerBase + T.
inline constexpr std::uint8_t kRegPrescalerBase = 0x40;

/// Auto-reload (period) register base. Timer T is kRegAutoReloadBase + T.
inline constexpr std::uint8_t kRegAutoReloadBase = 0x44;

/// MCU system clock feeding the timers.
inline constexpr std::uint32_t kClockHz = 72000000;

/// Each timer drives four PWM channels. Channels sharing a timer share PSC and ARR.
inline constexpr std::uint8_t kChannelsPerTimer = 4;

/// I2C address the MCU answers on. Some board revisions use kAltAddress instead.
inline constexpr std::uint8_t kDefaultAddress = 0x14;
inline constexpr std::uint8_t kAltAddress = 0x15;

/// Which timer a PWM channel belongs to.
/// NOTE: channels 12 and 13 share a timer, as do 8 and 9 -- so the four wheels
/// of this robot sit on only two timers, and PSC/ARR writes are shared.
constexpr std::uint8_t timer_for_channel(std::uint8_t channel)
{
    return static_cast<std::uint8_t>(channel / kChannelsPerTimer);
}

/// Registers are written big-endian as [reg, value_high, value_low].
inline constexpr std::size_t kRegisterWriteBytes = 3;

}  // namespace picar_hw::hat
