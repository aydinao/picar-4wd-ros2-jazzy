# picar_hw

Standalone C++ driver for the PiCar-4WD HAT. No ROS: builds with a compiler and
libgpiod v2 only. Wiring, bring-up and chip facts are in the
[repo README](../README.md).

## Layout

```
include/picar_hw/
  i2c_bus.hpp         I2cBus interface; LinuxI2cBus over /dev/i2c-N
  hat.hpp             the HAT's PWM: frequency per timer, duty per channel
  hat_registers.hpp   chip constants (register map, clock, timers)
  motor.hpp           one wheel: duty cycle + direction GPIO
  encoder.hpp         rising-edge counter on a background thread
  reset.hpp           pulse the MCU out of reset
tools/
  twitch.cpp          drive one wheel:  twitch <channel> <dir_gpio> <power -100..100> <s>
  encoder_probe.cpp   print both encoder counts once a second
test/
  test_hat.cpp        byte-level register tests
  fake_i2c_bus.hpp    I2cBus that records writes instead of sending them
```

## Boundary

Must never depend on `rclcpp`, `hardware_interface` or `pluginlib`;
`CMakeLists.txt` doesn't `find_package` them, so a breach fails the build.
Wiring arrives as constructor arguments.

## Tests

The tests assert on the bytes written to the wire. `Hat` takes an `I2cBus &`;
the tests hand it a `FakeI2cBus` that records each write, so no I2C hardware is
needed.

| Kind | Asserts |
|---|---|
| addressing | timer 3's PSC/ARR are `0x43`/`0x47`; channel 13's compare register is `0x2D` |
| contract | emitted registers decode to the requested frequency within 0.1%; PSC is the smallest that fits; 100% duty is CCR = ARR + 1 |
| characterisation | golden registers for 50 Hz (regression lock only) |
