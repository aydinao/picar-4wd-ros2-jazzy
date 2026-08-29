# picar_hw

Standalone C++ driver for the SunFounder PiCar-4WD HAT. **No ROS dependency** —
this package builds, links and runs with only a compiler and `libgpiod`.

For HAT bring-up (getting `0x14` to appear on the bus at all), the register map,
the STM32 findings and the encoder facts, see the [repo README](../README.md).

## Layout

```
picar_hw/
├── include/picar_hw/
│   ├── hat.hpp              PWM driver over the HAT's I2C registers
│   ├── hat_registers.hpp    chip constants (register map, clock, timers)
│   └── motor.hpp            one wheel: duty cycle + direction GPIO
├── src/{hat,motor}.cpp
├── test/
│   ├── temp_i2c_file.hpp    RAII temp file standing in for /dev/i2c-1
│   └── test_hat.cpp         byte-level register tests
└── tools/twitch.cpp         standalone smoke test
```

## Tests

```bash
pixi run test
```

The tests assert on the bytes the driver puts **on the wire**, since those are
the whole contract with the STM32. `Hat`'s filename constructor is pointed at a
temp file instead of `/dev/i2c-1`: the `ioctl(I2C_SLAVE)` fails on a regular
file but `write()` still runs, so the register writes land in order and can be
read back.

Three kinds, deliberately separate:

| Kind | Asserts | Derived from |
|------|---------|--------------|
| addressing | channel 13 writes timer 3's PSC/ARR (`0x43`/`0x47`) | the register map |
| contract | emitted registers decode to the requested frequency within 1% | `f = CLOCK / ((PSC+1)(ARR+1))` |
| characterisation | exact golden bytes for 50 Hz | current behaviour -- a regression lock, nothing more |

Each test needs a **unique** temp filename: I2CPP caches fds by filename in a
process-global singleton that is never cleared, so a reused path silently
returns an earlier test's fd. `mkstemp` handles this.

## The boundary

This package must never depend on `rclcpp`, `hardware_interface` or `pluginlib`.
`CMakeLists.txt` deliberately does not `find_package` any of them, so a breach is
a build error rather than a review comment. Verify with:

```bash
ldd install/picar_hw/lib/libpicar_hw.so | grep -i rclcpp   # expect no output
```

Robot *wiring* — channel and GPIO per wheel, polarity, wheel geometry — is not
compiled in here. It arrives as constructor arguments; `picar_ros` reads it from
the URDF.

## twitch

The first thing to reach for when the robot misbehaves: drives one wheel with no
ROS, no URDF and no `controller_manager` in the way.

```bash
# twitch <pwm_channel> <dir_gpio> <power -100..100> <seconds>
twitch 13 23  60 2     # left front,  forward
twitch 12 24 -60 2     # right front, reverse
```

| Wheel | PWM channel | Direction GPIO (BCM) |
|-------|-------------|----------------------|
| left front | 13 | 23 |
| right front | 12 | 24 |
| left rear | 8 | 13 |
| right rear | 9 | 20 |

Needs the HAT battery-powered and the MCU out of reset.

## Notes

- `libgpiod` **v2** API. The system package on Ubuntu 24.04 is 1.6.3, which is
  incompatible; build inside the pixi environment.
- Channels sharing a timer share prescaler and period: 12/13 are one timer, 8/9
  another. Setting a frequency on one wheel changes its sibling.
