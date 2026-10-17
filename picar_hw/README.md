# picar_hw

Standalone C++ driver for the PiCar-4WD HAT. No ROS: builds with a compiler and
libgpiod v2 only. Wiring, bring-up and chip facts are in the
[repo README](../README.md).

## Layout

```
include/picar_hw/
  hat.hpp             PWM over the HAT's I2C registers
  hat_registers.hpp   chip constants (register map, clock, timers)
  motor.hpp           one wheel: duty cycle + direction GPIO
  encoder.hpp         rising-edge counter on a background thread
  reset.hpp           pulse the MCU out of reset
tools/
  twitch.cpp          drive one wheel:  twitch <channel> <dir_gpio> <power -100..100> <s>
  encoder_probe.cpp   print both encoder counts once a second
test/test_hat.cpp     byte-level register tests
```

## Boundary

Must never depend on `rclcpp`, `hardware_interface` or `pluginlib`;
`CMakeLists.txt` doesn't `find_package` them, so a breach fails the build.
Wiring arrives as constructor arguments.

## Tests

The tests assert on the bytes written to the wire. `Hat` is pointed at a temp
file instead of `/dev/i2c-1`: `ioctl(I2C_SLAVE)` fails but `write()` still
lands.

| Kind | Asserts |
|---|---|
| addressing | channel 13 writes timer 3's PSC/ARR (`0x43`/`0x47`) |
| contract | emitted registers decode to the requested frequency within 1% |
| characterisation | golden bytes for 50 Hz (regression lock only) |

Each test needs a unique temp filename (`mkstemp`): I2CPP caches fds by
filename in a global singleton, so a reused path returns a stale fd.
