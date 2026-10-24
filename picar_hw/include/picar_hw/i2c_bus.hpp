#pragma once

#include <cstddef>
#include <cstdint>

namespace picar_hw
{

/// Something that sends bytes to one I2C device.
///
/// The seam between Hat and the kernel: on the robot it is LinuxI2cBus, in
/// the tests a fake that records what was sent.
class I2cBus
{
public:
  virtual ~I2cBus() = default;

  /// Send `size` bytes as one I2C write. Throws on failure.
  virtual void write(const std::uint8_t * data, std::size_t size) = 0;
};

/// /dev/i2c-N through the kernel's i2c-dev interface:
/// https://docs.kernel.org/i2c/dev-interface.html
class LinuxI2cBus : public I2cBus
{
public:
  /// Opens /dev/i2c-<bus> and selects the 7-bit `address` for every write.
  /// Throws std::system_error if either step fails.
  LinuxI2cBus(int bus, std::uint8_t address);
  ~LinuxI2cBus() override;

  // Owns a file descriptor; a copy would close it twice.
  LinuxI2cBus(const LinuxI2cBus &) = delete;
  LinuxI2cBus & operator=(const LinuxI2cBus &) = delete;
  LinuxI2cBus(LinuxI2cBus &&) = delete;
  LinuxI2cBus & operator=(LinuxI2cBus &&) = delete;

  void write(const std::uint8_t * data, std::size_t size) override;

private:
  int fd_ = -1;
};

}  // namespace picar_hw
