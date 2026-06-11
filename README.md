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

## C++ driver layer

A native C++ hardware abstraction lives in `src/` and `include/`, targeting the HAT without the Python dependency:

| File | Role |
|------|------|
| `include/4WDHAT.hpp` / `src/device/4WDHAT.cpp` | PWM driver — wraps the HAT's I2C registers |
| `include/motor.hpp` / `src/motor.cpp` | Motor controller — maps power (−100…100) to PWM duty cycle + GPIO direction pin |

### How the pin assignments were determined

SunFounder's Python motor init is the source of truth:

```python
left_front  = Motor(PWM("P13"), Pin("D4"))
right_front = Motor(PWM("P12"), Pin("D5"))
left_rear   = Motor(PWM("P8"),  Pin("D11"))
right_rear  = Motor(PWM("P9"),  Pin("D15"))
```

The PWM channel number is the integer in the `P` name (e.g. `P13` → channel `13`).  
The `D`-pin names are resolved to BCM GPIO numbers via `picar_4wd/pin.py`:

```python
"D4": 23, "D5": 24, "D11": 13, "D15": 20
```

The I2C address `0x14` was confirmed in §4. This gives the C++ constructor arguments directly:

```cpp
PiCar4WDHAT(/*bus*/ 1, /*addr*/ 0x14, /*channel*/ 13);  // left front PWM
Motor(pwm_obj, /*dir_pin BCM*/ 23);                      // left front direction
```

Dependencies: [`I2CPP`](libs/I2CPP/) (vendored), `libgpiod` (`sudo apt install libgpiod-dev`).

Build a specific target:
```bash
cmake -B build && cmake --build build --target test_motor
sudo ./build/test_motor
```

---

## Open items / next steps

- Confirm `picar-4wd test motor` runs end-to-end now that the MCU is reachable.
- Decide on dependency strategy for reproducibility (apt list + setup script, or container).
- Wrap the hardware layer in a thin ROS 2 node (`cmd_vel` → wheel speeds) once bring-up is stable.
- The HAT must be battery-powered; running the Pi from external supply only will produce
  `OSError: [Errno 121] Remote I/O error` on motor commands.


  