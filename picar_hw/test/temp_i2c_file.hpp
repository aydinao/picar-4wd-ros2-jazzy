#pragma once

#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

/// A throwaway file that stands in for the I2C device node.
///
/// `Hat`'s filename constructor opens this instead of /dev/i2c-1. The
/// ioctl(I2C_SLAVE) fails on a regular file, but write() still runs, so the
/// exact register writes land here in order and we can read them back.
///
/// Each instance MUST have a unique name: I2CPP caches fds by filename in a
/// process-global singleton that is never cleared, so reusing a path in a
/// second test would silently hand back the first test's fd. mkstemp gives
/// uniqueness for free (and creates the file atomically, unlike tmpnam).
class TempI2cFile
{
public:
    TempI2cFile()
    {
        const std::string tmpl = ::testing::TempDir() + "picar_i2c_XXXXXX";
        std::vector<char> buf(tmpl.begin(), tmpl.end());
        buf.push_back('\0');
        const int fd = ::mkstemp(buf.data());
        if (fd < 0) {
            throw std::runtime_error("mkstemp failed");
        }
        ::close(fd);   // I2CPP reopens it by name
        path_ = buf.data();
    }

    ~TempI2cFile() { ::unlink(path_.c_str()); }

    TempI2cFile(const TempI2cFile &) = delete;
    TempI2cFile & operator=(const TempI2cFile &) = delete;

    const std::string & path() const { return path_; }

    /// Every byte written so far, in order.
    std::vector<std::uint8_t> bytes() const
    {
        std::ifstream in(path_, std::ios::binary);
        return std::vector<std::uint8_t>(
            std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

private:
    std::string path_;
};

/// One [reg, value_high, value_low] write, decoded.
struct RegWrite
{
    std::uint8_t reg;
    std::uint16_t value;
    bool operator==(const RegWrite & o) const { return reg == o.reg && value == o.value; }
};

inline std::vector<RegWrite> decode(const std::vector<std::uint8_t> & raw)
{
    EXPECT_EQ(raw.size() % 3u, 0u) << "register writes are 3 bytes each";
    std::vector<RegWrite> out;
    for (std::size_t i = 0; i + 2 < raw.size(); i += 3) {
        out.push_back({raw[i], static_cast<std::uint16_t>((raw[i + 1] << 8) | raw[i + 2])});
    }
    return out;
}

inline std::ostream & operator<<(std::ostream & os, const RegWrite & w)
{
    return os << "{reg=0x" << std::hex << static_cast<int>(w.reg)
              << ", value=" << std::dec << w.value << "}";
}
