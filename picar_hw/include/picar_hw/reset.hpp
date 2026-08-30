#pragma once

#include <chrono>
#include <string>

namespace picar_hw
{

/// Release the HAT's onboard MCU from reset.
///
/// The 4WD-HAT holds its STM32 in reset until a GPIO line is pulsed LOW then
/// HIGH; until that happens the chip does not acknowledge on the I2C bus at
/// all and i2cdetect shows an empty grid. SunFounder's stack does this during
/// startup on Raspberry Pi OS, so on any other distribution it must be done
/// explicitly. See the repo README, "The actual blocker: the MCU is held in
/// reset".
///
/// @param chip_path  GPIO character device, e.g. /dev/gpiochip0
/// @param line       BCM line number of the reset pin (GPIO21 on this board)
/// @param low_time   how long to hold the line low
/// @param settle     how long to wait after releasing, before using the bus
/// @returns false if the chip or line could not be claimed
bool pulse_reset(
    const std::string & chip_path,
    unsigned int line,
    std::chrono::milliseconds low_time = std::chrono::milliseconds(100),
    std::chrono::milliseconds settle = std::chrono::milliseconds(500));

}  // namespace picar_hw
