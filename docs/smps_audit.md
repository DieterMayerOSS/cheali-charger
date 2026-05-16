# SMPS.cpp audit

Code-review of [`src/core/strategy/SMPS.cpp`](../src/core/strategy/SMPS.cpp)
(130 lines) and its public header [`SMPS.h`](../src/core/strategy/SMPS.h).
The module is the firmware's wrapper around the actual switching-mode
power supply: it owns the PWM setpoint, enforces per-call ramp limits,
gates on per-target hardware bounds, and toggles the charger output.

This document is a *read-only* audit — concrete fixes (where warranted)
should be split off as separate issues.

Closes the analysis part of #13.

## Public API surface

```cpp
namespace SMPS {
    void initialize();
    bool isPowerOn();       // returns on_
    bool isWorking();       // returns value_ != 0
    AnalogInputs::ValueType getIout();   // returns IoutSet_ (the requested I, not measured)
    void trySetIout(I);
    uint16_t getValue();
    void setValue(uint16_t);
    void powerOn();
    void powerOff();
}
```

Internal state (three plain — not volatile — variables):

| Variable | Type | Meaning |
|---|---|---|
| `on_`        | `bool`     | output relay state |
| `value_`     | `uint16_t` | last PWM duty written to hardware |
| `IoutSet_`   | `ValueType`| last requested current (the smoothed setpoint, in real-world units) |

## Findings

### 1. Setpoint clamping — partial enforcement

`SMPS::trySetIout(I)` performs three clamping layers before calling
`setValue`:

1. **Per-target hardware limit** via `getMaxIout()`, which uses
   `settings.maxIc` (max charge current) and `settings.maxPc`
   (max charge power, via `evalI(maxPc, Vout)`).
2. **Per-call ramp limit** of `SMPS_MAX_CURRENT_CHANGE_dM` =
   `SMPS_MAX_CURRENT_CHANGE × 0.7` = 140 mA (with the default
   `SMPS_MAX_CURRENT_CHANGE` of 200 mA).
3. **PWM bound** via `SMPS::setValue`'s clamp to `SMPS_UPPERBOUND_VALUE`
   (a per-target macro, e.g. `TIMER1_PRECISION_PERIOD/2` for
   Turnigy-MEGA-400Wx2).

The MAX_CHARGE_I limit is also clamped at the *settings* layer —
[Settings.cpp:118](../src/core/Settings.cpp) refuses to accept a
`settings.maxIc` greater than the target's `MAX_CHARGE_I`. So
`getMaxIout()` always returns a value within the hardware budget.

**Gap:** `Settings::check()` does **not** clamp `settings.maxPc`
against `MAX_CHARGE_P`. A user edit (or EEPROM corruption) that pushes
`maxPc` above the hardware's rated power would propagate through
`getMaxIout()` and pass to the PWM stage. The PWM still clamps to
`SMPS_UPPERBOUND_VALUE`, so absolute hardware destruction is unlikely,
but the hardware power budget isn't enforced in the same belt-and-
braces manner as the current budget.

Suggested follow-up: add `if (maxPc > MAX_CHARGE_P) maxPc = MAX_CHARGE_P;`
to `Settings::check()`. Trivial change; closing one half of an asymmetry.

### 2. Soft-start / rate-limit semantics

The 140 mA-per-call ramp limit (constant
`SMPS_MAX_CURRENT_CHANGE_dM`) applies to **every** `trySetIout` call,
not just the post-`powerOn` startup phase. This means:

- **On power-on**: `value_` and `IoutSet_` are reset to 0; the first
  `trySetIout(I)` can move I from 0 to at most 140 mA, the next call
  to at most 280 mA, etc. Effective soft-start rate = 140 mA × call
  frequency.
- **Mid-run**: the same rate limit applies. If `TheveninMethod` decides
  to drop current sharply (e.g. transition to CV tapering), the ramp
  is *enforced* — the SMPS will only step down by 140 mA per call.

The constant is overridable per target via
`#define SMPS_MAX_CURRENT_CHANGE`, but no target in the tree currently
redefines it. The default of 200 mA (and dM of 140 mA) is hard-coded.

Observations:

- This is a *non-obvious coupling* between the SMPS rate-limit and the
  Thevenin algorithm's expected step size. If `TheveninMethod::calculateNewI`
  computes a setpoint far from the previous one, only a portion of the
  change reaches the hardware per cycle. The Thevenin model assumes its
  computed setpoint was *applied*, so its Rth estimate may drift if the
  SMPS rate-limiter eats large requested changes.
- For end-of-charge tapering this is benign (changes are small and
  monotonic anyway), but for emergency shutdowns or strategy switches
  it could matter. Worth a separate investigation if the user observes
  oscillation during transitions.

No bug per se, but the rate-limit is undocumented and surprising. A
short comment block in `SMPS.cpp` explaining the rationale would help.

**Historical credit:** the "smooth current rising/falling to protect power
supplies" was one of the features added by [njozsef's 2015 fork](https://github.com/njozsef/cheali-charger-test1)
(`cheali-charger-test1`) and was subsequently merged into upstream. The
fork itself is marked obsolete by its author today, but the rate-limit
mechanism we see in current `SMPS.cpp` traces back there.

### 3. PWM resolution / frequency — not visible from SMPS.cpp

The actual PWM hardware setup is in `hardware::setChargerValue(value_)`,
which is per-target code under `src/hardware/.../cpu/`. `SMPS.cpp` only
sees the `SMPS_UPPERBOUND_VALUE` constant from `HardwareConfig.h` and a
raw `uint16_t` setpoint. Without descending into the AVR/Nuvoton timer
code:

- For Turnigy-MEGA-400Wx2 / Bantam-BC6HP-250W class targets,
  `SMPS_UPPERBOUND_VALUE = TIMER1_PRECISION_PERIOD / 2` (half-precision)
- For most other atmega32 targets, `SMPS_UPPERBOUND_VALUE =
  TIMER1_PRECISION_PERIOD` (full precision)

The 400W chargers halve the PWM resolution — presumably a deliberate
trade-off to allow higher PWM frequency at the cost of duty-cycle
granularity. This is the kind of decision that deserves a comment in
the target's HardwareConfig.h; currently there is none. (Tracking-worthy
but out of scope for this audit.)

### 4. Error handling — silent clamping, no error-reporting path

- `setValue(value > SMPS_UPPERBOUND_VALUE)` silently clamps to the
  upper bound. No `Program::stopReason` set, no log, no buzzer.
- `trySetIout(I > getMaxIout())` silently clamps to the hardware
  budget. Same.
- `hardware::setChargerValue(value_)` is `void`-returning — no signal
  back to SMPS.cpp if the underlying timer/peripheral somehow refused
  the write. Silent failure mode is possible if the timer hardware
  ever fails to commit.

For embedded code at this trust level (drives 800 W into a battery),
silent clamping is defensible *only* because the upstream callers
(TheveninMethod, the charge strategies) have already filtered their
requests through `Strategy::maxI` and per-cell sanity. The defence-in-
depth would be one more line: an assertion / breadcrumb stored in
`Program::stopReason` if `getMaxIout() < requested` by more than a
threshold for several consecutive cycles. Not currently present.

### 5. ISR-vs-main synchronisation — three plain (non-volatile) variables

`SMPS::on_`, `SMPS::value_`, `SMPS::IoutSet_` are all plain `bool` /
`uint16_t` / `ValueType` — not `volatile`, not wrapped in
`ATOMIC_BLOCK` accesses.

Who reads them?

| Reader | Path | ISR context? |
|---|---|---|
| `Monitor::doSlowInterrupt` (Monitor.cpp:200) | `SMPS::isWorking()` reads `value_` | **YES — interrupt** |
| `Monitor::run` | (indirect via TheveninMethod) | main loop |
| `ScreenMethods::displayStatus` | `SMPS::isPowerOn()` reads `on_` | main loop |
| `AnalogInputs::doSlowInterrupt` (chain to SMPS::isWorking) | indirect | **YES** |

Who writes them? Only `SMPS::setValue`, `SMPS::powerOn`,
`SMPS::powerOff`, `SMPS::trySetIout` — all called from the main loop
(strategy code).

So the access pattern is: **main loop writes, ISR reads, no
synchronisation**. On AVR-8 a `uint16_t` write is two byte-level
instructions, which means an ISR can observe a torn `value_` read.
For the specific read patterns:

- `value_ != 0` (the `isWorking()` test): a torn read produces either
  a partial low-byte or partial high-byte. The `!= 0` check would only
  be wrong if both halves of the new value are zero but the partial
  read is non-zero — i.e. if the writer is moving away from zero, the
  ISR sees a non-zero intermediate. In practice this matches the
  semantic intent ("are we currently driving current?"), so this
  particular reader is **accidentally safe**.
- `on_` is a single byte (bool) on AVR — atomic by construction. Safe.
- `IoutSet_` is read by `SMPS::getIout()` from the main loop only
  (calibration menu + screen). No ISR contention.

Conclusion: **no observable bug**, but the SMPS variables should still
be marked `volatile` for correctness and clarity. Future refactoring
that adds an ISR reader of `IoutSet_` or extends `value_`'s usage
would inherit a real race today.

### 6. Interaction with TheveninMethod — the missed-setpoint hypothesis

`TheveninChargeStrategy::doStrategy` calls
`SMPS::trySetIout(TheveninMethod::calculateNewI(...))` every cycle.
`calculateNewI` produces a smoothed new I via low-pass filtering and
state-machine transitions; it expects the SMPS to *apply* the computed
value as the new operating point.

The rate-limiter in `trySetIout` (finding §2) means a large requested
change is only partially applied per call. If the algorithm:

1. computes newI = 5.0 A while previous was 0.5 A (a delta of 4.5 A),
2. trySetIout clamps to 0.5 + 0.14 = 0.64 A,
3. on the next cycle, the Thevenin model sees actual Iout ≈ 0.64 A,
   not the requested 5.0 A,
4. the Rth estimate (which depends on Δv / Δi between consecutive
   samples) is fit to the *small* applied change, not the requested
   large change.

This is fine as long as the algorithm makes only small steps (the
typical CV-tapering case). It can produce a "lazy" startup ramp when
the strategy wants to bulk-charge at full current immediately. The
`SimpleChargeStrategy::doStrategy` calls `SMPS::trySetIout(Strategy::maxI)`
every cycle, so once at maxI the rate-limit doesn't kick in — but the
first ~50 cycles after `powerOn` are spent ramping up at 140 mA per
call.

Not a bug, but a documented design intent ("ramp up gracefully") that
deserves a one-line comment in `SMPS.cpp`.

### 7. Misc smaller observations

- `SMPS::initialize()` does:
  ```cpp
  value_ = 0;
  IoutSet_ = 0;
  setValue(0);
  on_ = true;
  powerOff();   // calls setValue(0) and sets on_ = false
  ```
  The `on_ = true` followed immediately by `powerOff()` looks like a
  bug at first read. It's actually a workaround for the `powerOff`
  guard `if(!isPowerOn()) return;` — without first setting `on_` to
  `true`, `powerOff` would early-out and the `hardware::setChargerOutput(false)`
  call wouldn't run. Easy to misread; a comment would help.
- `SMPS::setValue` calls `AnalogInputs::resetMeasurement()` on every
  write. Tightly couples the PWM update to the ADC subsystem — every
  setpoint change throws away accumulated ADC averages. The rationale
  ("new setpoint, old measurements stale") is sensible, but the
  coupling means SMPS depends on AnalogInputs's resetMeasurement
  semantics; refactoring either module risks breaking the other
  silently.
- `SMPS::getIout()` returns `IoutSet_` (the requested setpoint), not
  the *measured* output current — the docstring `//returns the truly
  set Iout` could be misread as "actually-measured". Callers wanting
  measured current must use `AnalogInputs::getIout()` (different
  function, different module).

## Action items (suggested as follow-up issues)

| # | Severity | Item |
|---|---|---|
| A | low | Clamp `settings.maxPc` against `MAX_CHARGE_P` in `Settings::check()` (finding §1) |
| B | low | Mark `SMPS::on_`, `value_`, `IoutSet_` as `volatile` (finding §5) |
| C | doc-only | Add a comment block in `SMPS.cpp` explaining the rate-limit constants and the `initialize()` workaround (findings §2, §7) |
| D | investigation | Confirm whether the rate-limit interacts pathologically with delta-V termination on NiMH packs (finding §2 + §6) |
| E | doc-only | Rename or re-document `SMPS::getIout()` so it's not confused with `AnalogInputs::getIout()` (finding §7) |

None of these are urgent. The module is small, comprehensible after
one read, and its behaviour is consistent. The main risk is the *un-
documented design intent* — every constant and ordering decision in
this file is deliberate, but a future maintainer has no way to know
that without spelunking. A few short comments would close most of the
audit findings cheaply.
