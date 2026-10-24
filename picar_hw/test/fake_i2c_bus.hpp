#pragma once

#include <gtest/gtest.h>

#include <cstdint>
#include <ostream>
#include <vector>

#include "picar_hw/i2c_bus.hpp"

/// Stands in for the I2C bus: records every write instead of sending it.
class FakeI2cBus : public picar_hw::I2cBus
{
public:
  void write(const std::uint8_t * data, std::size_t size) override
  {
    writes.emplace_back(data, data + size);
  }

  /// One entry per write() call, in order.
  std::vector<std::vector<std::uint8_t>> writes;
};

/// One [reg, value_high, value_low] write, decoded.
struct RegWrite
{
  std::uint8_t reg;
  std::uint16_t value;
  bool operator==(const RegWrite & o) const {return reg == o.reg && value == o.value;}
};

inline std::vector<RegWrite> decode(const FakeI2cBus & bus)
{
  std::vector<RegWrite> out;
  for (const auto & w : bus.writes) {
    EXPECT_EQ(w.size(), 3u) << "each register write is one 3-byte I2C write";
    if (w.size() == 3u) {
      out.push_back({w[0], static_cast<std::uint16_t>((w[1] << 8) | w[2])});
    }
  }
  return out;
}

inline std::ostream & operator<<(std::ostream & os, const RegWrite & w)
{
  return os << "{reg=0x" << std::hex << static_cast<int>(w.reg)
            << ", value=" << std::dec << w.value << "}";
}
