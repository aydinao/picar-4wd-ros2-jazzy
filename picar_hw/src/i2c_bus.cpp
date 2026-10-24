#include "picar_hw/i2c_bus.hpp"

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <string>
#include <system_error>

namespace picar_hw
{

LinuxI2cBus::LinuxI2cBus(int bus, std::uint8_t address)
{
  const std::string path = "/dev/i2c-" + std::to_string(bus);

  fd_ = ::open(path.c_str(), O_RDWR);
  if (fd_ < 0) {
    throw std::system_error(errno, std::generic_category(), "open " + path);
  }

  // Every later write() on this fd goes to `address`.
  if (::ioctl(fd_, I2C_SLAVE, address) < 0) {
    const int err = errno;      // close() may overwrite errno
    ::close(fd_);
    throw std::system_error(err, std::generic_category(), "ioctl(I2C_SLAVE) on " + path);
  }
}

LinuxI2cBus::~LinuxI2cBus()
{
  ::close(fd_);
}

void LinuxI2cBus::write(const std::uint8_t * data, std::size_t size)
{
  const ssize_t written = ::write(fd_, data, size);
  if (written < 0) {
    throw std::system_error(errno, std::generic_category(), "i2c write");
  }
  if (static_cast<std::size_t>(written) != size) {
    throw std::system_error(EIO, std::generic_category(), "i2c write: short write");
  }
}

}  // namespace picar_hw
