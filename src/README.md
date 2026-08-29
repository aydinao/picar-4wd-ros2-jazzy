# C++ Driver for the SunFounder PiCar-4WD HAT

Native C++ replacement for SunFounder's Python `picar-4wd` library, targeting the same
`4WD-HAT` board. The Python source is the only documentation that exists for this
hardware — the register map, timing calculations, and channel addressing here were read
from it directly. There is no public schematic.

This document covers building, using, and extending the C++ driver. For HAT bring-up
(getting `0x14` to appear on the bus in the first place), see the root README.

---

## Prerequisites

The HAT must already be reachable at `0x14` before using. If `i2cdetect -y 1`
does not show `14` in the grid, fix that first — see the root README, section "The actual
blocker: the MCU is held in reset".

Build dependencies (Ubuntu 24.04 on Pi):

```bash
sudo apt install -y build-essential cmake git i2c-tools gpiod libgpiod libgpiod-dev
```

Runtime: your user must be in the `i2c` group, otherwise every run needs `sudo`:

```bash
sudo usermod -aG i2c $USER
# log out and back in
```

## GPIO CLI Test

To list all GPIO controllers:

```bash
sudo gpiodetect

#gpiochip0 [pinctrl-bcm2711] (58 lines)
#gpiochip1 [raspberrypi-exp-gpio] (8 lines)

```
List all pin information for the gpiochip0 controller

```bash
gpioinfo 0
```

---

## Repo layout

```
picar-4wd-ros2-jazzy/
├── CMakeLists.txt
├── include/
│   ├── 4WDHAT.hpp           // PWM driver header
│   └── motor.hpp            // motor controller header
├── src/
│   ├── main.cpp             // PWM test harness
│   ├── motor.cpp            // motor controller implementation
│   ├── test_motor.cpp       // 4-wheel motor test harness
│   └── device/
│       └── 4WDHAT.cpp       // PWM driver implementation
└── libs/
    └── I2CPP/               // vendored I2C library (see Dependencies)
```

Include paths are set by CMake — source files reference headers as `"4WDHAT.hpp"` and
`"i2cpp/device.hpp"`, **not** `"include/4WDHAT.hpp"` or
`"libs/I2CPP/include/i2cpp/device.hpp"`. Don't bake directory structure into source.

---

## Dependencies

[`I2CPP`](https://github.com/mwaverecycling/I2CPP) — a thin C++ wrapper over Linux's
`/dev/i2c-*` interface. Vendored as a subdirectory rather than installed system-wide so
the build is self-contained.

```bash
git clone https://github.com/mwaverecycling/I2CPP.git libs/I2CPP
```

`PiCar4WDHAT` inherits from `i2cpp::Device`, which provides protected `read_i2c()` and
`write_i2c()` helpers that wrap the bus file descriptor and device address. The driver
never touches `ioctl` directly.

[`libgpiod`](https://libgpiod.readthedocs.io) — the standard Linux GPIO character device
library. Used by `Motor` to drive the direction pin on each wheel.

```bash
sudo apt install libgpiod-dev
```

---

## Building

```bash
mkdir -p build && cd build
cmake ..
make -j4
./picar_test
```

Re-run `cmake ..` only when `CMakeLists.txt` changes. Everyday edits to `.cpp`/`.hpp`
files just need `make`.

---

## Quick start

After running the executable the PiCar wheel should start spinning. Executing the programme while the wheel is spinning will result in the wheel spinning forever. This is a limitation of the test main.cpp currently. Exit the programme when it is not moving. 

---

## API

All members live in namespace `PiCar_4WD`.

### `Motor`

```cpp
Motor(PiCar4WDHAT& pwm, uint8_t dir_pin, bool is_reversed = false);
```

| Param         | Meaning                                              |
|---------------|------------------------------------------------------|
| `pwm`         | A `PiCar4WDHAT` instance for this wheel's channel   |
| `dir_pin`     | BCM GPIO number for the direction line               |
| `is_reversed` | Flip direction logic (e.g. for physically mirrored wheels) |

| Method                        | Effect                                              |
|-------------------------------|-----------------------------------------------------|
| `set_power(int8_t power)`     | Drive motor at −100…100. 0 = stop, negatives reverse. Power is scaled: any non-zero value is mapped to 50–100% duty cycle to overcome motor stiction. |

The destructor releases the GPIO line and closes the chip handle automatically.

---

### `PiCar4WDHAT`

```cpp
PiCar4WDHAT(int bus, uint_fast8_t address, uint8_t channel);
```

| Param     | Typical value | Meaning                                             |
|-----------|---------------|-----------------------------------------------------|
| `bus`     | `1`           | I2C adapter number (`/dev/i2c-1` on the Pi header)  |
| `address` | `0x14`        | MCU address. Some board revisions sit at `0x15`     |
| `channel` | `0`–`13`      | PWM channel (`P0`–`P13` in the SunFounder docs)     |

Unlike the Python `PWM.__init__`, the C++ constructor does **not** auto-probe `0x14`
then `0x15`, and does **not** implicitly call `set_frequency(50)`. Callers do both
explicitly.

### Public methods

| Method                                  | Effect                                            |
|-----------------------------------------|---------------------------------------------------|
| `set_frequency(uint16_t hz)`            | Compute and write best (prescaler, period) pair   |
| `set_pulse_width(uint16_t value)`       | Set raw on-time count on this channel             |
| `set_duty_cycle(float percent)`         | Set on-time as 0.0–100.0 % of cached period       |

## Implementation notes

### Register map

Recovered from `picar_4wd` library source:

| Constant  | Address | Purpose                              |
|-----------|---------|--------------------------------------|
| `REG_CHN` | `0x20`  | Channel select (per-channel pulse)   |
| `REG_FRE` | `0x30`  | Frequency                            |
| `REG_PSC` | `0x40`  | Prescaler (timer clock divider)      |
| `REG_ARR` | `0x44`  | Auto-reload register (period)        |

MCU clock is 72 MHz. Each timer covers 4 channels, so `timer = channel / 4` and the
register offset is `REG_PSC + timer` for the prescaler, `REG_ARR + timer` for the
period.

All register writes are three bytes: `[reg, value_high, value_low]` — **big-endian**.

### PWM frequency calculation

PWM frequency is set by finding an integer prescaler/period pair whose product best
approximates `CLOCK / frequency`. Because `CLOCK / frequency` is rarely a perfect
square, a brute-force search over ten prescaler candidates centred on
`sqrt(CLOCK / frequency)` finds the pair with minimum frequency error.

### Python parity: the `-1` adjustment

The Python library stores prescaler and period as `n - 1` before writing to the bus,
to match the underlying MCU timer convention (the registers count from 0). The C++
driver mirrors this:

```cpp
prescaler = prescaler - 1;   // written to REG_PSC
period_ = period - 1;        // cached and written to REG_ARR
```

`set_duty_cycle` multiplies the cached `period_` (the corrected value) by the duty
fraction. If you bypass `set_period` and write `REG_ARR` by hand, `set_duty_cycle`
will be off-by-one until you do.

---

## Troubleshooting

| Symptom                                          | Likely cause                                                |
|--------------------------------------------------|-------------------------------------------------------------|
| `fatal error: 4WDHAT.hpp: No such file...`       | Include paths not set — check `target_include_directories` |
| `fatal error: include/4WDHAT.hpp: No such file`  | Source still has `"include/..."` prefix — drop it          |
| `Permission denied` opening `/dev/i2c-1`         | User not in `i2c` group, or didn't re-login after `usermod` |
| `i2cdetect` shows nothing at `0x14`              | MCU in reset — see root README, not a driver issue          |
| Motor spins forever after Ctrl+C                 | Missing signal handler / destructor not running             |
| Motor spins but ignores direction                | Direction GPIO not driven — PWM alone doesn't pick direction |
| `OSError: Remote I/O error` (when run via Python sanity check) | HAT not on battery — Pi-only power is insufficient for motor commands |
| Frequency seems wildly wrong                      | Prescaler truncation — confirm prescaler param is `uint16_t` |

---

## Known limitations

- **No `SIGKILL` safety for `PiCar4WDHAT`.** Nothing stops a spinning wheel on `kill -9` at the PWM level.
- **No address auto-probe.** Construction takes the address as a parameter. If your
  board sits at `0x15` instead of `0x14`, pass `0x15`.
- **No `SIGKILL` safety.** Nothing catches `kill -9`; the wheel will keep spinning.
  A hardware watchdog or MCU-side timeout would be needed for true fail-safe operation.
- **No thread safety.** Single-threaded use only. Concurrent calls from multiple
  threads will corrupt the bus state in `i2cpp::Device`.

---

## Roadmap

- [x] Motor wrapper class — combines PWM channel + direction GPIO into one `Motor`.
- [ ] Servo wrapper class — angle in degrees → pulse width, with calibration.
- [ ] ADC read support — the same MCU at `0x14` also exposes the battery voltage and
  line-follower readings.
- [ ] ROS 2 Jazzy node — `cmd_vel` subscriber → per-wheel speed → driver calls.
- [ ] Optional: address auto-probe (`0x14` then `0x15`) matching the Python init.