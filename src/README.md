## C++ Driver

With the HAT reliably reachable at `0x14` on every boot, the Python library is being replaced with a native C++ driver. The Python source served as the reference — the register map, timing calculations, and channel addressing were read from it directly rather than from any schematic (none exists publicly for this board).

### Register map

Recovered from `picar_4wd` library source:

| Constant | Address | Purpose |
|----------|---------|---------|
| `REG_CHN` | `0x20` | Channel select |
| `REG_FRE` | `0x30` | Frequency |
| `REG_PSC` | `0x40` | Prescaler (timer clock divider) |
| `REG_ARR` | `0x44` | Auto-reload register (period) |

The MCU internal clock is 72 MHz.

### PWM frequency calculation

PWM frequency is set by finding an integer prescaler/period pair whose product best approximates `CLOCK / frequency`. Because `CLOCK / frequency` is rarely a perfect square, a brute-force search over ten prescaler candidates centred on `sqrt(CLOCK / frequency)` finds the pair with minimum frequency error.

### Structure

`PiCar4WDHAT` inherits from a thin `Device` base class wrapping Linux I2C file descriptor operations. It lives in the `PiCar_4WD` namespace.

### Current state

| Method | Status |
|--------|--------|
| `set_frequency` | ✅ Implemented |
| `set_prescaler` | 🔲 Pending |
| `set_period` | 🔲 Pending |
| `set_pulse_width` | 🔲 Pending |
| `set_duty_cycle` | 🔲 Pending |