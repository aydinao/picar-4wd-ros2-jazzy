// Byte-level tests for the HAT PWM driver.
//
// These assert on what goes ON THE WIRE, because those bytes are the entire
// contract with the STM32. Expected values are derived from the STM32 timer
// relationship (RM0008, general-purpose timers)
//
//     f = CLOCK / ((PSC_reg + 1) * (ARR_reg + 1))      duty = CCR / (ARR_reg + 1)
//
// and from the register map in hat_registers.hpp -- NOT by running the driver
// and recording what it happened to emit. Where a test is a pure regression
// lock on current behaviour, it says so.
#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <system_error>

#include "fake_i2c_bus.hpp"
#include "picar_hw/hat.hpp"
#include "picar_hw/hat_registers.hpp"

namespace hw = picar_hw::hat;

namespace
{
/// Reconstruct the output frequency the chip would produce from the raw
/// register values written. This is the physics, independent of how the
/// driver chose them.
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

TEST(Hat, ConstructorSetsPrescalerThenPeriodOfEveryTimer)
{
  FakeI2cBus bus;
  picar_hw::Hat hat(bus, 50);

  const auto writes = decode(bus);
  ASSERT_EQ(writes.size(), 2u * hw::kTimerCount);
  for (std::uint8_t t = 0; t < hw::kTimerCount; ++t) {
    EXPECT_EQ(writes[2 * t].reg, hw::kRegPrescalerBase + t) << "timer " << int{t};
    EXPECT_EQ(writes[2 * t + 1].reg, hw::kRegAutoReloadBase + t) << "timer " << int{t};
  }
}

TEST(Hat, SetFrequencyWritesOnlyThatTimer)
{
  FakeI2cBus bus;
  picar_hw::Hat hat(bus, 50);
  bus.writes.clear();

  hat.set_frequency(3, 200);

  const auto writes = decode(bus);
  ASSERT_EQ(writes.size(), 2u);
  EXPECT_EQ(writes[0].reg, hw::kRegPrescalerBase + 3) << "PSC for timer 3 is 0x43";
  EXPECT_EQ(writes[1].reg, hw::kRegAutoReloadBase + 3) << "ARR for timer 3 is 0x47";
}

TEST(Hat, RegisterValuesAreWrittenBigEndian)
{
  FakeI2cBus bus;
  picar_hw::Hat hat(bus, 50);

  // Timer 0's ARR at 50 Hz is 65453 = 0xFFAD: high byte first.
  ASSERT_GE(bus.writes.size(), 2u);
  const std::vector<std::uint8_t> expected{0x44, 0xFF, 0xAD};
  EXPECT_EQ(bus.writes[1], expected);
}

// --- the contract: do the emitted values mean the requested frequency? -------

TEST(Hat, EmittedRegistersProduceTheRequestedFrequency)
{
  for (const std::uint32_t requested : {1u, 50u, 100u, 200u, 500u, 1000u, 20000u}) {
    FakeI2cBus bus;
    picar_hw::Hat hat(bus, requested);

    const auto writes = decode(bus);
    ASSERT_GE(writes.size(), 2u) << "requested " << requested;
    const double actual = frequency_from(writes[0].value, writes[1].value);
    const double error = std::abs(actual - requested) / requested;
    EXPECT_LT(error, 0.001) << "requested " << requested << " Hz, registers give "
                            << actual << " Hz";
  }
}

TEST(Hat, PrescalerIsTheSmallestThatLetsThePeriodFit)
{
  for (const std::uint32_t requested : {1u, 50u, 100u, 200u, 500u, 1000u, 20000u}) {
    FakeI2cBus bus;
    picar_hw::Hat hat(bus, requested);

    const auto writes = decode(bus);
    ASSERT_GE(writes.size(), 2u);
    const std::uint32_t psc = writes[0].value;
    const std::uint32_t ticks = hw::kClockHz / requested;

    EXPECT_LE(ticks / (psc + 1), hw::kRegisterMax) << requested << " Hz: period must fit";
    if (psc > 0) {
      EXPECT_GT(ticks / psc, hw::kRegisterMax)
        << requested << " Hz: one prescaler step lower would also have fit";
    }
  }
}

TEST(Hat, PeriodLeavesRoomForFullDuty)
{
  FakeI2cBus bus;
  picar_hw::Hat hat(bus, 50);

  const auto writes = decode(bus);
  ASSERT_GE(writes.size(), 2u);
  EXPECT_LT(writes[1].value, hw::kRegisterMax) << "CCR = ARR + 1 must fit in 16 bits";
}

TEST(Hat, FrequencyOutsideTheRangeThrows)
{
  FakeI2cBus bus;
  EXPECT_THROW(picar_hw::Hat(bus, 0), std::invalid_argument);
  EXPECT_THROW(picar_hw::Hat(bus, picar_hw::Hat::kMaxFrequencyHz + 1), std::invalid_argument);
  EXPECT_NO_THROW(picar_hw::Hat(bus, picar_hw::Hat::kMaxFrequencyHz));
}

TEST(Hat, UnknownTimerThrows)
{
  FakeI2cBus bus;
  picar_hw::Hat hat(bus, 50);
  EXPECT_THROW(hat.set_frequency(hw::kTimerCount, 50), std::out_of_range);
}

// --- duty cycle -------------------------------------------------------------

TEST(Hat, SetDutyCycleWritesTheChannelsCompareRegister)
{
  FakeI2cBus bus;
  picar_hw::Hat hat(bus, 50);
  bus.writes.clear();

  hat.set_duty_cycle(13, 50.0f);

  const auto writes = decode(bus);
  ASSERT_EQ(writes.size(), 1u);
  EXPECT_EQ(writes[0].reg, hw::kRegChannelBase + 13) << "compare register for P13 is 0x2D";
}

TEST(Hat, FullDutyHoldsTheOutputHighForTheWholePeriod)
{
  FakeI2cBus bus;
  picar_hw::Hat hat(bus, 50);
  const std::uint16_t arr = decode(bus)[2 * 3 + 1].value;      // timer 3's ARR
  bus.writes.clear();

  hat.set_duty_cycle(13, 100.0f);
  hat.set_duty_cycle(13, 0.0f);

  const auto writes = decode(bus);
  ASSERT_EQ(writes.size(), 2u);
  EXPECT_EQ(writes[0].value, arr + 1) << "100% needs CCR = ARR + 1, not ARR";
  EXPECT_EQ(writes[1].value, 0u);
}

TEST(Hat, DutyCycleClampsToTheClosedRangeZeroToOneHundred)
{
  FakeI2cBus bus;
  picar_hw::Hat hat(bus, 50);
  const std::uint16_t arr = decode(bus)[2 * 3 + 1].value;
  bus.writes.clear();

  hat.set_duty_cycle(13, 250.0f);
  hat.set_duty_cycle(13, -40.0f);

  const auto writes = decode(bus);
  ASSERT_EQ(writes.size(), 2u);
  EXPECT_EQ(writes[0].value, arr + 1) << "over 100% should saturate at the full period";
  EXPECT_EQ(writes[1].value, 0u) << "negative should saturate at zero";
}

TEST(Hat, DutyCycleUsesItsOwnTimersPeriod)
{
  FakeI2cBus bus;
  picar_hw::Hat hat(bus, 50);
  hat.set_frequency(3, 20000);            // 72 MHz / 20 kHz = 3600 counts, PSC 0
  bus.writes.clear();

  hat.set_duty_cycle(13, 50.0f);          // timer 3
  hat.set_duty_cycle(8, 50.0f);           // timer 2, still 50 Hz

  const auto writes = decode(bus);
  ASSERT_EQ(writes.size(), 2u);
  EXPECT_EQ(writes[0].value, 1800u);
  EXPECT_EQ(writes[1].value, 32727u) << "half of timer 2's 65454 counts";
}

TEST(Hat, ChannelsSharingATimerSeeItsNewPeriod)
{
  // Channels 12 and 13 share timer 3. Changing its frequency must change the
  // scale of both: neither may keep a stale copy of the old period.
  FakeI2cBus bus;
  picar_hw::Hat hat(bus, 50);
  hat.set_frequency(3, 200);
  bus.writes.clear();

  hat.set_duty_cycle(12, 50.0f);
  hat.set_duty_cycle(13, 50.0f);

  const auto writes = decode(bus);
  ASSERT_EQ(writes.size(), 2u);
  EXPECT_EQ(writes[0].value, writes[1].value);
  EXPECT_EQ(writes[0].value, 30000u) << "half of 60000 counts at 200 Hz";
}

TEST(Hat, ChannelOnNoTimerThrows)
{
  FakeI2cBus bus;
  picar_hw::Hat hat(bus, 50);
  EXPECT_THROW(
    hat.set_duty_cycle(hw::kTimerCount * hw::kChannelsPerTimer, 50.0f),
    std::out_of_range);
}

// --- characterisation: locks current behaviour, not derived from physics -----

TEST(Hat, GoldenRegistersForFiftyHertz)
{
  FakeI2cBus bus;
  picar_hw::Hat hat(bus, 50);

  // ticks = 72e6 / 50 = 1440000; PSC = 1440000 / 65536 = 21;
  // period = 1440000 / 22 = 65454 counts, written as ARR = 65453.
  // 72e6 / (22 * 65454) = 50.0004 Hz.
  const auto writes = decode(bus);
  ASSERT_GE(writes.size(), 2u);
  const std::vector<RegWrite> expected{{0x40, 21}, {0x44, 65453}};
  EXPECT_EQ(std::vector<RegWrite>(writes.begin(), writes.begin() + 2), expected);
}

// --- the real bus -------------------------------------------------------------

TEST(LinuxI2cBus, MissingAdapterThrows)
{
  EXPECT_THROW(picar_hw::LinuxI2cBus(255, hw::kDefaultAddress), std::system_error);
}
