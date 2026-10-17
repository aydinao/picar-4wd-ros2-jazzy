# PiCar-4WD on Ubuntu 24.04 + ROS 2 Jazzy

`ros2_control` hardware interface for the SunFounder **PiCar-4WD** (`4WD-HAT`,
not the Robot HAT) on a Raspberry Pi 4B running Ubuntu 24.04.

| Package | Contents | ROS dependency |
|---|---|---|
| [`picar_hw`](picar_hw/) | PWM, motor, encoder and reset drivers; `twitch` and `encoder_probe` tools | none (libc, libstdc++, libgpiod) |
| `picar_ros` | `SystemInterface` plugin, URDF (`description/`), launch and controller config (`bringup/`) | hardware_interface, pluginlib |

Robot wiring lives in the URDF as parameters; only facts about the chip are
compiled in.

## Quickstart

Toolchain is RoboStack via pixi (no `sudo`, x86_64 and aarch64 from one
manifest). Always build inside pixi: its libgpiod is 2.x, Ubuntu's is 1.6.3.

```bash
pixi install
pixi run build        # colcon build; add --packages-select <pkg> for one
pixi run test         # fails on red
pixi run ci-local     # build a fresh clone of HEAD, as CI sees it (commit first)
pixi run clean
```

```bash
pixi shell && source install/setup.bash
ros2 launch picar_ros picar.launch.py use_mock_hardware:=true
ros2 topic pub --once /picar_base_controller/cmd_vel \
  geometry_msgs/msg/TwistStamped '{twist: {linear: {x: 0.1}}}'
ros2 control list_hardware_interfaces   # all four wheel commands [claimed]
```

Drive one wheel with no ROS: `install/picar_hw/lib/picar_hw/twitch 13 23 60 2`.

## HAT bring-up on Ubuntu

SunFounder's `setup.py` assumes Raspberry Pi OS (`raspi-config`) and fails on
Ubuntu. The real blocker is that the HAT's MCU is **held in reset until BCM
GPIO21 is pulsed LOW→HIGH** — SunFounder's `soft_reset()` does this via pin
`D16`, which maps to GPIO21, not 16. Until then `i2cdetect -y 1` is empty.

1. In `/boot/firmware/config.txt`, before any `dtoverlay=` line:
   `dtparam=i2c_arm=on` and `dtparam=spi=on`. Reboot.
2. `sudo apt install i2c-tools python3-smbus python3-rpi.gpio` (apt, not pip,
   to keep ROS's system Python intact).
3. Install the boot-time reset from [`system_utils/`](system_utils/):
   ```bash
   sudo cp system_utils/picar-hat-reset.py /usr/local/bin/
   sudo cp system_utils/picar-hat-reset.service /etc/systemd/system/
   sudo systemctl daemon-reload && sudo systemctl enable --now picar-hat-reset
   ```
4. Reboot; `i2cdetect -y 1` should show `14`.

The HAT must be battery-powered; on external Pi power alone motor commands
fail with `OSError: [Errno 121] Remote I/O error`.

## Hardware facts

Derived from SunFounder's `picar-4wd` Python source, then verified wheel by
wheel with `twitch` and `encoder_probe` (the rear pair is swapped relative to
the Python); there is no public schematic.

| Wheel | PWM channel | Direction GPIO (BCM) |
|---|---|---|
| left front | 13 | 23 |
| right front | 12 | 24 |
| left rear | 9 | 20 |
| right rear | 8 | 13 |

- **MCU:** STM32F103 at I2C `0x14`. Its firmware exposes `TIMx->PSC/ARR/CCRy`,
  so `f = 72 MHz / ((PSC+1)(ARR+1))` (RM0008).
- **Timers are shared:** channels 12/13 are one timer, 8/9 another. Setting
  one wheel's frequency changes its sibling's.
- **Drive:** skid-steer, not Ackermann. Four motors → four joints, two per side
  for `diff_drive_controller`.
- **Encoders:** two, 20 slots, single channel (magnitude only). BCM 25 = left,
  BCM 4 = right, and each turns with its side's **rear** wheel only — the front
  wheels are unmeasured. Direction comes from the commanded direction pin.
- **Wheel radius:** ~3.3 cm, unmeasured. SunFounder labels its speed in mm/s;
  the label is wrong.

## Status

**Working:** HAT bring-up on every boot · PWM and motor drivers verified on
hardware · plugin builds in CI and activates with mock hardware ·
`write()` drives the motors from `cmd_vel` · motors stop on deactivate ·
`picar_hw::Encoder` counts edges.

**Not yet:**
- `read()` — four joints declare velocity state, two sensors exist (rear
  wheels only); open modelling decision.
- `max_wheel_speed_rad_s` and all URDF dimensions are placeholders.
- Servo and ADC channels.
- Nothing stops the motors on `SIGKILL` or battery loss.
