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
└── tools/twitch.cpp         standalone smoke test
```

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
