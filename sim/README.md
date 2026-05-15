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

1. **ETA timer subtraction order** — `Monitor::calculateDeltaProcentTimeSec`
   had reversed operands since 2013, underflowing to `~UINT32_MAX` and
   making the displayed ETA garbage.
2. **uint16 underflow in `isCalibrationRequired`** — empty connection
   mask underflowed `Vmax - Vmin` to 1; guarded with an early return.
3. **Dead state `RthMesurment`** — declared in the `TheveninMethod`
   State enum with an unreachable switch case; removed.
4. **Div-by-zero in `calibrateValue` / `reverseCalibrateValue`** —
   degenerate calibration (`p0.x == p1.x` or `p0.y == p1.y`) was UB;
   guarded with safe-zero return.
5. **Late calibration error reporting** — `Calibration::check()` (which
   catches inverted voltage/current points via `if(adcMax <= adcMin)`)
   was only invoked from `Program::run`, so users only saw the error
   when they tried to start a charge program. Now also runs on
   calibration menu exit, giving immediate feedback.

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

## Build (MSYS2 UCRT64)

```
cmake -S sim -B sim/build -G Ninja
cmake --build sim/build
ctest --test-dir sim/build --output-on-failure
```

PowerShell needs the toolchain on PATH first:
`$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"`.
