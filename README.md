# SunFounder PiCar-4WD on Ubuntu 24.04 + ROS 2 Jazzy

Notes for bringing up the SunFounder **PiCar-4WD** (the `4WD-HAT`, not the newer Robot HAT) on a
Raspberry Pi 4B running **Ubuntu 24.04 LTS** with **ROS 2 Jazzy** installed system-wide.

SunFounder's tooling assumes Raspberry Pi OS. On Ubuntu it fails in several places. This documents
what breaks, why, and the fix that actually works — ending with a one-time `systemd` service that
makes the HAT come up reliably on every boot.

> Hardware note: the `4WD-HAT` is a discontinued board with effectively no public schematic. The
> facts below were verified by reading the installed `picar-4wd` library source and testing against
> the hardware, not from a datasheet.

---

## TL;DR

1. SunFounder's `setup.py` calls `raspi-config`, which does not exist on Ubuntu.
2. I2C/SPI must be enabled manually via `/boot/firmware/config.txt`.
3. A few Python deps (`smbus`, `RPi.GPIO`) must be installed via `apt`, not the broken installer.
4. **The real blocker:** the HAT's onboard MCU (I2C address `0x14`) is held in reset until
   **GPIO21** is pulsed LOW→HIGH. On Raspberry Pi OS the SunFounder stack handles this; on Ubuntu
   nothing does, so the chip never appears on the bus.
5. Fix: a `systemd` oneshot service that pulses GPIO21 on boot.

---

## Environment

| Item | Value |
|------|-------|
| Board | Raspberry Pi 4B |
| OS | Ubuntu 24.04 LTS (64-bit) |
| ROS 2 | Jazzy, system-wide |
| HAT | SunFounder PiCar-4WD `4WD-HAT` |
| Onboard MCU | I2C address `0x14` (PWM + ADC controller) |
| Power | 2× Li-ion (HAT must be battery-powered) |

---

## 1. SunFounder's installer assumes Raspberry Pi OS

Running `sudo python3 setup.py install` errors out on these lines:

```python
do(msg="turn on I2C", cmd='sudo raspi-config nonint do_i2c 0')
do(msg="turn on SPI", cmd='sudo raspi-config nonint do_spi 0')
```

`raspi-config` is a Raspberry Pi OS tool and is not present on Ubuntu. The installer also tries to
upgrade `pip` in a way that conflicts with Debian-managed `pip`. Net result: dependency installation
and interface setup do not complete.

**Takeaway:** do not rely on `setup.py` for system configuration on Ubuntu. Do the equivalent steps
manually (below).

---

## 2. Enable I2C and SPI manually

Edit the firmware config:

```bash
sudo nano /boot/firmware/config.txt
```

Ensure these lines are present **before** any `dtoverlay=` lines (base device-tree params must come
first on Ubuntu):

```
dtparam=i2c_arm=on
dtparam=spi=on
```

Install the I2C userspace tools and reboot:

```bash
sudo apt install i2c-tools
sudo reboot
```

Verify the bus exists and identify the controllers:

```bash
i2cdetect -l
```

```
i2c-1   i2c   bcm2835 (i2c@7e804000)   I2C adapter   <- GPIO header (pins 3/5), this is the one
i2c-20  i2c   fef04500.i2c             I2C adapter   <- internal (HDMI/DSI), ignore
i2c-21  i2c   fef09500.i2c             I2C adapter   <- internal (HDMI/DSI), ignore
```

> If `i2c_arm_baudrate` is ever needed (e.g. for clock-stretching devices), it must be on the same
> `dtparam` line: `dtparam=i2c_arm=on,i2c_arm_baudrate=10000`. A bare `i2c_arm_baudrate=10000` line
> is silently ignored.

---

## 3. Install the Python dependencies the installer missed

The `picar-4wd` library imports modules not covered by a clean Ubuntu install:

```bash
sudo apt install python3-smbus
sudo apt install python3-rpi.gpio
```

Use `apt` (system packages) rather than `pip --break-system-packages`, so the system Python that
ROS 2 depends on is left intact.

---

## 4. The actual blocker: the MCU is held in reset

After all of the above, `i2cdetect -y 1` still showed an **empty grid** — nothing at `0x14`:

```
ubuntu@ubuntu:~$ i2cdetect -y 1
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:                         -- -- -- -- -- -- -- --
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
... (all empty) ...
```

### Why this is not a software/config problem

`i2cdetect` is a C program that talks to the kernel directly. If a device is missing from its
output, the cause is at the bus/hardware layer — not the Python environment. This narrows the search:

- `i2cdetect -l` confirmed `i2c-1` is the correct `bcm2835` GPIO-header controller. ✅ bus correct
- Multimeter: the HAT's I2C header `3V3`/`SDA`/`SCL` pins were live. ✅ HAT powered, lines pulled up
- Continuity confirmed `SDA`/`SCL` reach the MCU through the HAT PCB. ✅ wiring intact
- A direct `smbus2` `write_byte(0x14, ...)` got no ACK. ❌ MCU not responding

Everything was correct *except the MCU itself was not answering* — which points to it being held in
**reset**.

### The reset line: D16 → GPIO21

Reading the library source revealed the reset mechanism. `picar_4wd/utils.py`:

```python
def soft_reset():
    from .pin import Pin
    soft_reset_pin = Pin("D16")
    soft_reset_pin.low()
    time.sleep(0.01)
    soft_reset_pin.high()
    time.sleep(0.01)
```

And `picar_4wd/pin.py` maps the board's logical pin names to BCM GPIO numbers:

```python
"D16": 21,   # D16 is GPIO21, NOT GPIO16
```

So the MCU reset is **BCM GPIO21** (physical pin 40), pulsed LOW→HIGH.

On Raspberry Pi OS the SunFounder stack performs this pulse during install/runtime, so the chip is
released from reset before anything uses it. On Ubuntu nothing does this, so the MCU sits in reset
indefinitely and never appears on the bus.

### Manual proof

```bash
sudo python3 -c "
import RPi.GPIO as GPIO
import time, smbus2
GPIO.setmode(GPIO.BCM)
GPIO.setup(21, GPIO.OUT)
GPIO.output(21, GPIO.LOW)
time.sleep(0.1)
GPIO.output(21, GPIO.HIGH)
time.sleep(0.5)
bus = smbus2.SMBus(1)
for addr in [0x14, 0x15]:
    try:
        bus.write_byte(addr, 0x2C)
        print(f'ACK at 0x{addr:02X}!')
    except OSError:
        print(f'No response at 0x{addr:02X}')
GPIO.cleanup()
"
```

```
ACK at 0x14!
No response at 0x15
```

The chip is alive. The HAT was never faulty.

---

## 5. Permanent fix: reset the HAT on every boot

### Reset script

```bash
sudo nano /usr/local/bin/picar-hat-reset.py
```

```python
import RPi.GPIO as GPIO
import time

GPIO.setmode(GPIO.BCM)
GPIO.setup(21, GPIO.OUT)
GPIO.output(21, GPIO.LOW)
time.sleep(0.1)
GPIO.output(21, GPIO.HIGH)
time.sleep(0.5)
GPIO.cleanup()
```

### systemd service

```bash
sudo nano /etc/systemd/system/picar-hat-reset.service
```

```ini
[Unit]
Description=PiCar 4WD HAT Reset
Before=multi-user.target

[Service]
Type=oneshot
ExecStart=/usr/bin/python3 /usr/local/bin/picar-hat-reset.py
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
```

> Note: put the Python in a script file as above. A multi-line `ExecStart=` inline in the unit file
> fails with "bad unit file setting".

### Enable and start

```bash
sudo systemctl daemon-reload
sudo systemctl enable picar-hat-reset.service
sudo systemctl start picar-hat-reset.service
sudo systemctl status picar-hat-reset.service
```

### Verify after reboot

```bash
sudo reboot
# then:
i2cdetect -y 1
```

```
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:                         -- -- -- -- -- -- -- --
10: -- -- -- -- 14 -- -- -- -- -- -- -- -- -- -- --
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
... (rest empty) ...
```

`0x14` now appears automatically on every boot, with no manual intervention.

---

## ros2_control hardware package

The C++ layer is split into two packages so the driver is usable without ROS:

| Package | Contents | ROS dependency |
|---------|----------|----------------|
| `picar_hw` | PWM driver, motor control, encoders, chip constants, and a standalone `twitch` tool | **none** |
| `picar_ros` | `SystemInterface` plugin, URDF (`description/`), launch and controller config (`bringup/`) | hardware_interface, pluginlib |

`picar_hw` links only libc, libstdc++ and libgpiod — it builds and tests without
a ROS installation, and works on any board wired to the same HAT. `picar_ros` is
a thin wrapper: lifecycle, `read()`, `write()`, nothing else.

Drive a single wheel with no ROS in the loop:

```bash
# twitch <pwm_channel> <dir_gpio> <power -100..100> <seconds>
install/picar_hw/lib/picar_hw/twitch 13 23 60 2
```

Bring the stack up (no robot needed with mock hardware):

```bash
ros2 launch picar_ros picar.launch.py use_mock_hardware:=true
ros2 topic pub /picar_base_controller/cmd_vel geometry_msgs/msg/TwistStamped \
  '{twist: {linear: {x: 0.1}}}'
```

The robot is **skid-steer**, not Ackermann -- SunFounder's `turn_left` drives
the left side backwards. Four independently commanded motors means four joints,
grouped two per side for `diff_drive_controller`. Only two encoders exist, one
per side, so per-wheel velocity is not measurable.

Robot *wiring* — which PWM channel and BCM pin each wheel uses — lives in the
URDF as parameters, not as C++ constants. Only facts about the chip itself are
compiled in.

### The HAT's MCU is an STM32

`REG_PSC` and `REG_ARR` are STM32 timer register names (prescaler, auto-reload),
72 MHz is the STM32F103's maximum system clock, and 4 timers × 4 channels
accounts for the 16 PWM channels the board exposes. The firmware is exposing
`TIMx->PSC`, `TIMx->ARR` and `TIMx->CCRy` over I2C, so ST's **RM0008** timer
chapter documents the far side of the bus:

```
f = CLOCK / ((PSC + 1) × (ARR + 1))
```

That `+ 1` is also why the Python writes `n - 1` before every register write —
the STM32 timer registers count from zero.

**Consequence:** channels sharing a timer share PSC and ARR. Channels 12 and 13
are one timer, 8 and 9 another — so the four wheels sit on only two timers, and
their PWM frequencies are not independent.

### How the pin assignments were determined

SunFounder's Python motor init is the source of truth:

```python
left_front  = Motor(PWM("P13"), Pin("D4"))
right_front = Motor(PWM("P12"), Pin("D5"))
left_rear   = Motor(PWM("P8"),  Pin("D11"))
right_rear  = Motor(PWM("P9"),  Pin("D15"))
```

The PWM channel number is the integer in the `P` name (`P13` → channel `13`).
The `D`-pin names resolve to BCM GPIO numbers via `picar_4wd/pin.py`:

```python
"D4": 23, "D5": 24, "D11": 13, "D15": 20
```

The I2C address `0x14` was confirmed in §4.

### Encoders

From `picar_4wd/speed.py`, the slotted discs on the motors are real encoders:

| Fact | Value |
|------|-------|
| Encoder GPIO | **BCM 25** and **BCM 4** |
| Slots per revolution | **20** (18° resolution) |
| Wheel radius | ~3.3 cm — see note below |
| Channels | **One** — magnitude only, no direction |

Two encoders for four motors, so per-wheel velocity is not measurable; direction
is inferred from the commanded direction pin, not sensed.

> SunFounder's `test3` prints its result as `mm/s`, but `2 * pi * 3.3 * rps` only
> yields mm/s for a 3.3 **mm** radius. The label is wrong — the radius is in cm.
> Measure it before trusting either.

---

## Building

The toolchain is **RoboStack via pixi** — ROS 2 Jazzy with no `sudo` and no
`apt`, from one manifest that resolves on both x86_64 and the Pi's aarch64:

```bash
pixi install
pixi run build      # both packages
pixi run test       # gtest suite (fails the build on red)
pixi run ci-local   # build a fresh clone of HEAD, as CI sees it
pixi run clean      # remove build/, install/, log/
```

`ci-local` clones the committed tree, so commit before running it. It catches
the "works on my machine" class of failure -- untracked files, missing
submodules -- without waiting on GitHub.

Note that pixi's `libgpiod` is **2.x**, while Ubuntu 24.04's system package is
1.6.3. The driver targets the v2 API, so build inside pixi on both machines.

---

## Status

**Working**
- HAT bring-up on Ubuntu 24.04 (§1–5) — the MCU appears at `0x14` on every boot
- PWM driver: frequency, pulse width, duty cycle, verified against hardware
- Motor control: four wheels driven forward under their own power
- Builds green as a `ros2_control` package on x86_64 and in CI
- Full bring-up verified with mock hardware: `controller_manager` loads the
  component, `diff_drive_controller` claims all four wheel velocity commands

**Not yet**
- `picar_system.cpp` lifecycle, `read()` and `write()` bodies -- the plugin
  loads and activates, but does nothing
- Encoder edge counting via libgpiod events
- Every dimension in the URDF is a placeholder; measure the robot
- Servo and ADC (battery, line-follower) channels

**Gotcha:** the HAT must be battery-powered. Running the Pi from an external
supply alone produces `OSError: [Errno 121] Remote I/O error` on motor commands.
