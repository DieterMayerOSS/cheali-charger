# Watchdog-Timer audit

Paper audit of whether the firmware sets up an AVR Watchdog Timer (WDT)
and kicks it during the main strategy loop, per issue #14.

## Finding: there is no watchdog.

Exhaustive grep across `src/core/` and `src/hardware/atmega32/`:

| Searched for | Hits |
|---|---|
| `<avr/wdt.h>` include | **0** |
| `wdt_enable(...)` | **0** |
| `wdt_reset()` | **0** |
| `wdt_disable()` | **0** |
| `WDTCSR` register access | **0** |
| `WDT_vect` ISR handler | **0** |
| `MCUSR` read (to detect a prior watchdog reset) | **0** |

The AVR Watchdog Timer module is **completely unused** by the firmware.

### Boot path

[`src/hardware/atmega32/cpu/cpu.h`](../src/hardware/atmega32/cpu/cpu.h):
```cpp
namespace cpu {
    inline void init() {
        sei();
    }
}
```

`cpu::init()` only enables global interrupts. No WDT setup, no fuse-side
configuration.

### Fuse-side configuration

The per-target `progUSBasp.sh` scripts only **read** fuses for backup
purposes:

```bash
avrdude -p$PARTNO -c$PROGRAMMER $TTY -Uflash:r:flash.bin:r -Ulfuse:r:lfuse.bin:r -Uhfuse:r:hfuse.bin:r ...
```

Neither the high nor low fuse is programmed by the build. The user's
charger therefore boots with whatever fuses the factory shipped — which
on ATmega32 means `WDTON = 1` (unprogrammed), i.e. the watchdog is
**software-controlled**, not always-on. Since the firmware never turns
it on, it stays off forever.

## Implications

- A hang anywhere in the strategy loop (`Strategy::doStrategy`, the
  ADC averaging in `AnalogInputs::doFullMeasurement`, any infinite
  blocking inside an analyzer call) will **never auto-recover**.
- The charger output relay state at the moment of hang is sticky.
  If the firmware was driving an SMPS PWM duty just before hanging,
  the timer hardware will keep generating PWM autonomously until
  the user manually pulls power.
- A specific known-hang scenario: `Keyboard::getPressedWithDelay`
  contains a `do { ... } while(delay <= currentStateDelay)` loop
  with `Time::delayDoIdle`. If `Time` somehow stops advancing
  (broken Timer0 ISR, missing interrupt vector after refactoring),
  this loop never exits.

## Severity assessment

For a 200 W / 400 W class charger:

- **Pro for "ok to ship without WDT":** The strategy loop is small and
  reviewable; the safety-stop paths in `Monitor::run` cover the
  realistic failure modes (battery disconnect, overcurrent, overtemp,
  PSU sag). The hardware-side SMPS clamps current via the analog
  feedback loop; software hang at a fixed PWM duty is mostly bounded.
- **Con for "ok":** Embedded best practice for battery-charging
  hardware is to have a WDT period of 1-8 seconds. The cost is
  trivial (one `wdt_enable(WDTO_4S)` at boot, one `wdt_reset()` per
  strategy cycle). Skipping it is a defensive engineering gap, not a
  bug, but the gap is unjustified given how cheap it would be to close.

## Suggested follow-up

Spin off as a separate issue (small, well-scoped):

> **Add WDT_enable(WDTO_4S) at boot + wdt_reset() inside the strategy loop**
>
> File: `src/hardware/atmega32/cpu/cpu.h` (add `wdt_enable(WDTO_4S)` to `cpu::init()`)
> File: `src/core/strategy/Strategy.cpp` (add `wdt_reset()` to the `do { } while` loop)
> File: `src/core/strategy/Balancer.cpp` (the balancing inner loop in `startBalancing()` / `doStrategy()`)
> File: `src/core/calibration/Calibration.cpp` (the testVout connect-wait loop has its own `while(true)`)
>
> Hardware verification required before deploying — a WDT timeout that's
> too short relative to a legitimate slow ADC-pass would brick perfectly
> good runs.

Closes the read-only part of #14. Issue stays open as the umbrella for
the suggested follow-up.
