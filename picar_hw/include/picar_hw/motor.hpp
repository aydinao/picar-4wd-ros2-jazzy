#pragma once

#include <cstdint>
#include <string>

#include "picar_hw/hat.hpp"

struct gpiod_chip;
struct gpiod_line_request;

namespace picar_hw
{

/// One wheel: a PWM channel for speed and a GPIO line for direction.
class Motor
{
public:
  /// @param hat         the HAT driving the wheel; must outlive this Motor
  /// @param channel     the wheel's PWM channel on the HAT
  /// @param chip_path   GPIO character device, e.g. /dev/gpiochip0
  /// @param dir_line    BCM line number of the direction pin
  /// @param is_reversed invert the direction line (mirrored mounting)
  Motor(
    Hat & hat, std::uint8_t channel, const std::string & chip_path, unsigned int dir_line,
    bool is_reversed = false);

  /// Sets the duty to zero. Never throws: a failed write is reported on stderr.
  ~Motor();

  // Owns a GPIO line request; a copy would release it twice.
  Motor(const Motor &) = delete;
  Motor & operator=(const Motor &) = delete;
  Motor(Motor &&) = delete;
  Motor & operator=(Motor &&) = delete;

  /// -100..100; the sign selects direction. Throws if the I2C write fails.
  void set_power(int8_t power);

  /// True if the direction line was successfully claimed.
  bool ok() const {return dir_request_ != nullptr;}

private:
  Hat & hat_;
  std::uint8_t channel_;
  unsigned int dir_line_;
  bool is_reversed_;

  // libgpiod v2: a chip handle plus a line request that owns the line.
  gpiod_chip * chip_ = nullptr;
  gpiod_line_request * dir_request_ = nullptr;
};

}  // namespace picar_hw
