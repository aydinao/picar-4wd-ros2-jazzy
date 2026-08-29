// Byte-level tests for the HAT PWM driver.
//
// These assert on what goes ON THE WIRE, because those bytes are the entire
// contract with the STM32. Expected values are derived from the STM32 timer
// relationship
//
//     f = CLOCK / ((PSC_reg + 1) * (ARR_reg + 1))
//
// and from the register map recovered from SunFounder's Python -- NOT by
// running the driver and recording what it happened to emit. Where a test is
// a pure regression lock on current behaviour, it says so.
#include <gtest/gtest.h>

#include <cmath>

#include "picar_hw/hat.hpp"
#include "picar_hw/hat_registers.hpp"
#include "temp_i2c_file.hpp"

namespace hw = picar_hw::hat;

namespace
{
constexpr std::uint8_t kAddr = hw::kDefaultAddress;

/// Reconstruct the output frequency the chip would produce from the raw
/// register values written. This is the physics, independent of how the
/// driver searched for them.
double frequency_from(std::uint16_t psc_reg, std::uint16_t arr_reg)
{
    return static_cast<double>(hw::kClockHz) /
           ((static_cast<double>(psc_reg) + 1.0) * (static_cast<double>(arr_reg) + 1.0));
}
}  // namespace

// --- register addressing ----------------------------------------------------

TEST(HatRegisters, TimerIsChannelDividedByFour)
{
    static_assert(hw::timer_for_channel(0) == 0, "");
    static_assert(hw::timer_for_channel(8) == 2, "");
    static_assert(hw::timer_for_channel(9) == 2, "");
    static_assert(hw::timer_for_channel(12) == 3, "");
    static_assert(hw::timer_for_channel(13) == 3, "");
    SUCCEED();
}

TEST(Hat, SetFrequencyWritesPrescalerThenPeriodOfTheChannelsTimer)
{
    TempI2cFile wire;
    picar_hw::Hat hat(wire.path(), kAddr, /*channel=*/13);   // channel 13 -> timer 3
    hat.set_frequency(50);

    const auto writes = decode(wire.bytes());
    ASSERT_EQ(writes.size(), 2u);
    EXPECT_EQ(writes[0].reg, hw::kRegPrescalerBase + 3) << "PSC for timer 3 is 0x43";
    EXPECT_EQ(writes[1].reg, hw::kRegAutoReloadBase + 3) << "ARR for timer 3 is 0x47";
}

TEST(Hat, RearWheelChannelsAddressTimerTwo)
{
    TempI2cFile wire;
    picar_hw::Hat hat(wire.path(), kAddr, /*channel=*/8);    // channel 8 -> timer 2
    hat.set_frequency(50);

    const auto writes = decode(wire.bytes());
    ASSERT_EQ(writes.size(), 2u);
    EXPECT_EQ(writes[0].reg, hw::kRegPrescalerBase + 2);
    EXPECT_EQ(writes[1].reg, hw::kRegAutoReloadBase + 2);
}

// --- the contract: do the emitted values actually mean 50 Hz? ----------------

TEST(Hat, SetFrequencyEmitsValuesThatProduceTheRequestedFrequency)
{
    for (const std::uint16_t requested : {50, 100, 200, 500, 1000}) {
        TempI2cFile wire;
        picar_hw::Hat hat(wire.path(), kAddr, 13);
        hat.set_frequency(requested);

        const auto writes = decode(wire.bytes());
        ASSERT_EQ(writes.size(), 2u) << "requested " << requested;

        const double actual = frequency_from(writes[0].value, writes[1].value);
        const double error = std::abs(actual - requested) / requested;
        EXPECT_LT(error, 0.01) << "requested " << requested << " Hz, registers give "
                               << actual << " Hz";
    }
}

TEST(Hat, RegisterValuesAreWrittenBigEndian)
{
    TempI2cFile wire;
    picar_hw::Hat hat(wire.path(), kAddr, 13);
    hat.set_frequency(50);

    const auto raw = wire.bytes();
    ASSERT_EQ(raw.size(), 6u);
    // 1194 = 0x04AA: high byte first.
    EXPECT_EQ(raw[1], 0x04);
    EXPECT_EQ(raw[2], 0xAA);
}

// --- pulse width / duty cycle -----------------------------------------------

TEST(Hat, SetDutyCycleWritesTheChannelsCompareRegister)
{
    TempI2cFile wire;
    picar_hw::Hat hat(wire.path(), kAddr, /*channel=*/13);
    hat.set_frequency(50);
    hat.set_duty_cycle(50.0f);

    const auto writes = decode(wire.bytes());
    ASSERT_EQ(writes.size(), 3u);
    EXPECT_EQ(writes[2].reg, hw::kRegChannelBase + 13) << "compare register for P13 is 0x2D";
}

TEST(Hat, DutyCycleIsAFractionOfTheCachedPeriod)
{
    TempI2cFile wire;
    picar_hw::Hat hat(wire.path(), kAddr, 13);
    hat.set_frequency(50);
    hat.set_duty_cycle(50.0f);

    const auto writes = decode(wire.bytes());
    ASSERT_EQ(writes.size(), 3u);
    const std::uint16_t arr = writes[1].value;      // period the driver cached
    EXPECT_EQ(writes[2].value, static_cast<std::uint16_t>(arr / 2))
        << "50% duty should be half the period";
}

TEST(Hat, DutyCycleClampsToTheClosedRangeZeroToOneHundred)
{
    TempI2cFile wire;
    picar_hw::Hat hat(wire.path(), kAddr, 13);
    hat.set_frequency(50);
    hat.set_duty_cycle(250.0f);
    hat.set_duty_cycle(-40.0f);

    const auto writes = decode(wire.bytes());
    ASSERT_EQ(writes.size(), 4u);
    EXPECT_EQ(writes[2].value, writes[1].value) << "over 100% should saturate at full period";
    EXPECT_EQ(writes[3].value, 0u) << "negative should saturate at zero";
}

// --- characterisation: locks current behaviour, not derived from physics -----

TEST(Hat, GoldenBytesForFiftyHertzOnChannelThirteen)
{
    TempI2cFile wire;
    picar_hw::Hat hat(wire.path(), kAddr, 13);
    hat.set_frequency(50);

    // PSC=1195, ARR=1205 -> written as n-1: 1194 (0x04AA), 1204 (0x04B4).
    // 72e6 / (1195 * 1205) = 50.001 Hz.
    const std::vector<RegWrite> expected{{0x43, 1194}, {0x47, 1204}};
    EXPECT_EQ(decode(wire.bytes()), expected);
}

// --- the shared-timer coupling, documented as a test ------------------------

TEST(Hat, ChannelsSharingATimerWriteTheSamePrescalerRegister)
{
    TempI2cFile wire_a;
    TempI2cFile wire_b;
    picar_hw::Hat left_front(wire_a.path(), kAddr, 13);
    picar_hw::Hat right_front(wire_b.path(), kAddr, 12);

    left_front.set_frequency(50);
    right_front.set_frequency(200);

    const auto a = decode(wire_a.bytes());
    const auto b = decode(wire_b.bytes());
    ASSERT_EQ(a.size(), 2u);
    ASSERT_EQ(b.size(), 2u);

    EXPECT_EQ(a[0].reg, b[0].reg) << "channels 13 and 12 share timer 3's prescaler";
    EXPECT_NE(a[0].value, b[0].value)
        << "so the second call silently changes the first channel's frequency";
}
