# sim/ — Host-side build for learning & testing

Standalone CMake project that compiles and tests algorithms from the
cheali-charger firmware on the host (no AVR/ARM toolchain needed).
Built originally as a learning vehicle for embedded C++ patterns and
as a test net for understanding the charging math before touching real
hardware. Now also runs as a regression net for the small bug-fixes
that have been applied to `src/` in this fork.

The ports under `algorithms/` are kept verbatim with the firmware
semantics — when `src/` is fixed, the corresponding port and tests are
updated in lockstep. The tests pin down both correct behaviour and
historical quirks that were deliberately preserved.

## Layout

```
sim/
├── CMakeLists.txt      # standalone host build, GCC + Ninja
├── algorithms/         # ports of firmware algorithms (pure C++)
├── tests/              # unit tests (no framework — plain asserts)
└── README.md           # this file
```

## Modules

| Module                       | Firmware origin                                | Tests |
|------------------------------|------------------------------------------------|------:|
| `find_min_cell`              | `Balancer::getCellMinV`                        |     5 |
| `thevenin`                   | `Thevenin` + `Resistance`                      |    13 |
| `balancer_logic`             | various `Balancer::*` helpers                  |    15 |
| `monitor_eta`                | `Monitor` ETA + percent calculation            |    12 |
| `charge_strategy_decisions`  | Simple / Delta / Thevenin charge decisions     |    16 |
| `monitor_safety`             | `Monitor::run` 9-check safety gate             |    19 |
| `calibration`                | `AnalogInputs::calibrateValue` + reverse       |    13 |
| `thevenin_fsm`               | TheveninMethod state machine, extracted        |    14 |

Total: **107 test cases**.

## Firmware fixes applied via this scaffold

Each fix has a corresponding test that pins down the corrected
behaviour. See the relevant commit message for context.

1. **ETA timer subtraction order** — `Monitor::calculateDeltaPercentTimeSec`
   had reversed operands since 2013, underflowing to `~UINT32_MAX` and
   making the displayed ETA garbage.
2. **uint16 underflow in `isCalibrationRequired`** — empty connection
   mask underflowed `Vmax - Vmin` to 1; guarded with an early return.
3. **Dead state `RthMeasurement`** — declared in the `TheveninMethod`
   State enum with an unreachable switch case; removed.
4. **Div-by-zero in `calibrateValue` / `reverseCalibrateValue`** —
   degenerate calibration (`p0.x == p1.x` or `p0.y == p1.y`) was UB;
   guarded with safe-zero return.
5. **Late calibration error reporting** — `Calibration::check()` (which
   catches inverted voltage/current points via `if(adcMax <= adcMin)`)
   was only invoked from `Program::run`, so users only saw the error
   when they tried to start a charge program. Now also runs on
   calibration menu exit, giving immediate feedback.
6. **Per-cell calibration not validated** — `Calibration::checkAll()`
   previously only checked the total-pack pin (`Vout_plus_pin`) and
   current channels, leaving the individual cell pins (`Vb1_pin`..
   `Vb6_pin`) silently accepted even with degenerate calibration.
   Now loops over `MAX_BALANCE_CELLS` and validates each with a
   1–4 V range. `Vb0_pin` is intentionally not checked (factory
   defaults leave it uncalibrated).
7. **User preference clobbered by calibration menu** — entering
   `Calibration::run()` unconditionally set `enable_externT = 0`,
   and `externalTemperatureCalibration()` set it to `1`. Neither
   restored the original value, so the user's external-temperature-
   monitoring preference was silently flipped depending on which
   submenu they last visited. Both call sites now save and restore
   the flag around their runtime use, preserving the stored
   preference. (Author had `//TODO: rewrite` on these lines.)
8. **SerialLog disabled on Mega 400Wx2** — the charger PCB has no
   TXD/RXD pins exposed, so the SerialLog facility was writing to
   a wire nobody listens to, occupying ~2 KB Flash and ~280 B SRAM
   (`tx_buffer` ring buffer + `Serial0` state). `ENABLE_SERIAL_LOG`
   was already a config switch but unconditional in `GlobalConfig.h`;
   per-target `#undef` plus a small guard in `HardwareSerial.cpp`
   now drops the entire serial stack when not needed. Targets with
   accessible serial pins (e.g. imaxB6-original) still build with
   SerialLog enabled. Mega 400Wx2 main build went from 94.6% to
   88.3% Flash and 51.5% to 37.3% SRAM.

Version bumped to 3.0 to mark this maintenance pass as a distinct
fork-state from upstream 2.02.

## Findings documented but deliberately NOT fixed

These are pinned down by tests so any future change must be conscious.

- **Discontinuity at x=0** in `calibrateValue`: the `if (x == 0) return 0`
  short-circuit makes the linear curve jump unless calibration passes
  through the origin. Possibly intentional against extrapolation
  underflow.
- **Silent inversion in `calibrateValue` itself**: not enforced in the
  math function, by design. Some sensors (NTC thermistors in voltage-
  divider configurations on Tintern/Textern) legitimately have a
  negative slope, so a monotonicity check inside the hot-path math
  would break valid temperature calibrations. The firmware already
  validates voltage/current monotonicity at a higher level via
  `Calibration::check()` (see fix #5 above).
- **Diametric overvoltage handling**: `SimpleChargeStrategy` returns
  `Error`, `DeltaChargeStrategy` returns `Complete`. Likely intentional
  (NiMH/NiCd treat -dV/overvoltage as normal terminus).
- **Sign-mismatch rejection** in `Thevenin::calculateRth`: real safety
  logic against measurement glitches, not a bug.
- **Duplicate `CC_Balancing → CC` transition** between
  `balance_isComplete` and `calculateNewI`: subtle ordering dependency,
  refactoring would require care.
- **Two-point linear calibration limit**: the documented cause of
  midrange drift on non-linear sensors (Mega 400Wx2 high-current
  shunts). A three-point upgrade is real work and would need hardware
  verification.

## Hardware-only diagnostic builds (Mega 400Wx2)

Two helper firmware targets that run the on-board diagnostic
analyzers instead of the normal charger logic — no SMPS output,
no battery drive, just ADC readings on the LCD. Useful for
verifying fixes on the real hardware without serial-port access.

- `Turnigy-MEGA-400Wx2-BalancePortAnalyzer_atmega32` — cycles
  through Vb0_pin..Vb6_pin, V+_pin, V-_pin raw ADC values;
  toggle balancer discharge per cell from the menu.
- `Turnigy-MEGA-400Wx2-AnalogInputsAnalyzer_atmega32` —
  full inventory of every physical ADC channel (voltages,
  currents, internal/external temperature, balance-port pins).

Both targets use the same Mega 400Wx2 pin layout, power limits,
and factory calibration defaults as the normal firmware, so
flashing them does not invalidate stored calibration data.
Flash back to `Turnigy-MEGA-400Wx2_atmega32.hex` to return to
normal operation.

## Build (MSYS2 UCRT64)

```
cmake -S sim -B sim/build -G Ninja
cmake --build sim/build
ctest --test-dir sim/build --output-on-failure
```

PowerShell needs the toolchain on PATH first:
`$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"`.
