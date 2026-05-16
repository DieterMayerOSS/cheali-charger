# EEPROM recovery / corruption-handling audit

Paper audit of `src/core/eeprom.cpp` (and supporting code) per issue #15.
Subject: what does the firmware do at boot / program-start if the EEPROM
contents have been corrupted (CRC mismatch, magic-number mismatch,
architecture mismatch, etc.).

## Summary

The firmware runs a **three-tier verification** of the EEPROM at every
boot (via `ChealiCharger2.cpp::main()`) and again at every program
start (via `Calibration::check()` in `Program::run`). The tiers are
checked in order:

1. **Magic-number tier** — fixed signature bytes that prove the EEPROM
   was ever written by this firmware.
2. **Architecture tier** — identifier tying the EEPROM to one specific
   target hardware + cell count.
3. **Per-section CRC + version tier** — the three independently-versioned
   sections (calibration, programData, settings) each carry a CRC-16
   over their data plus a version number.

On any mismatch, an **interactive recovery prompt** appears on the LCD
asking the user to confirm a reset. After confirmation, the affected
sections are restored from their PROGMEM defaults.

Verdict: the corruption handling is **substantially better than I
expected** for a firmware of this age. The interactive prompt is the
weakest part (blocks a headless boot forever), but everything else is
well-thought-out.

## EEPROM layout

From `src/core/eeprom.h` and the `Data` struct (the actual layout
lives across multiple headers, this is the conceptual view):

```
+--------------------------+
| magicString[4] = "chli"  |  4 B  signature
| architecture (u16)       |  2 B  CHEALI_CHARGER_ARCHITECTURE constant
| architectureInfo (u16)   |  2 B  MAX_BALANCE_CELLS as fingerprint
| calibrationVersion (u16) |  2 B  e10 — the "10" part of "e10.3.12"
| programDataVersion (u16) |  2 B  the "3" part
| settingVersion (u16)     |  2 B  the "12" part
+--------------------------+
| calibration[PHYSICAL_INPUTS] | (per-channel CalibrationPoints)
| CRC16 of calibration    |  2 B
+--------------------------+
| battery (ProgramData)   |  ~30 B
| CRC16 of battery        |  2 B
+--------------------------+
| settings                |  ~30 B
| CRC16 of settings       |  2 B
+--------------------------+
```

The three sections (calibration / battery / settings) are
**independently CRC-protected**. A corruption in one doesn't force a
reset of the others (modulo the cascade rule, see §3 below).

## Tier 1 — magic numbers + architecture

`testOrRestore(restore)` in `eeprom.cpp:48` checks four fixed values:

| Field | Expected value | Source |
|---|---|---|
| `magicString[0..1]` | `'c','h'` | hard-coded literal |
| `magicString[2..3]` | `'l','i'` | hard-coded literal |
| `architecture` | `CHEALI_CHARGER_ARCHITECTURE` | per-target macro = CPU + GENERIC bits |
| `architectureInfo` | `CHEALI_CHARGER_ARCHITECTURE_INFO` | `MAX_BALANCE_CELLS` |

If any of the four differ from the expected value, `EEPROM_RESTORE_MAGIC_NUMBER`
is set in the `test` mask. Notably this means **swapping the chip between
different charger targets is detected** — a Mega 400Wx2 EEPROM moved
to an iMaxB6 will refuse to load and prompt for reset, because their
`CHEALI_CHARGER_ARCHITECTURE` constants differ.

## Tier 2 — per-section versions

Each section carries its own version number, baked in at build time from
`CMakeLists.txt`:

- `cheali-charger-eeprom-calibration-version 10`
- `cheali-charger-eeprom-programdata-version 3`
- `cheali-charger-eeprom-settings-version 12`

If the on-EEPROM version doesn't match the firmware-side constant, that
section's restore flag is set. **This is the firmware-upgrade path** —
when an EEPROM layout change is intentional, the corresponding version
is bumped, and existing user EEPROMs are migrated by reset to defaults
on the next boot.

## Tier 3 — CRC-16 per section

`testOrRestoreCRC(adr, size, restore)` in `eeprom.cpp:120` computes a
CRC-16 (polynomial 0xA001 — that's the CRC-16/IBM-3740 a.k.a. Modbus)
across `size` bytes starting at `adr`, then compares to a stored CRC
immediately after the section.

The CRC routine is the standard byte-at-a-time CRC-16 (`crc16_update`
in `eeprom.cpp:100`), starting from 0xFFFF. No table lookup — runtime
is O(N×8) which is fine for the small sections involved.

Three section-specific helpers:

- `restoreCalibrationCRC` — covers all PHYSICAL_INPUTS calibration points
- `restoreProgramDataCRC` — covers `battery` struct
- `restoreSettingsCRC` — covers `settings` struct

## Tier 4 — bonus: read-retry on transient errors

`testOrRestore(adr, version, restore)` in `eeprom.cpp:35` retries the
EEPROM read up to **5 times** with 100 ms delays between attempts. This
handles transient read errors (low voltage during read, EMI from the
SMPS switching, etc.) before declaring a mismatch.

```cpp
uint8_t trials = EEPROM_READ_TRIALS;   // = 5
while(--trials) {
    if(eeprom::read(adr) == version)
        return false;
    Time::delay(100);
}
return true;
```

Defensive: a one-shot read glitch won't trigger a recovery prompt.

## Cascading reset logic

`eeprom::restoreDefault(uint8_t what)` in `eeprom.cpp:72` widens the
reset mask before applying:

```cpp
if(what & EEPROM_RESTORE_MAGIC_NUMBER)  what |= EEPROM_RESTORE_CALIBRATION;
if(what & EEPROM_RESTORE_CALIBRATION)   what |= EEPROM_RESTORE_PROGRAM_DATA;
if(what & EEPROM_RESTORE_PROGRAM_DATA)  what |= EEPROM_RESTORE_SETTINGS;
```

Rationale (inferred — not commented in source):

- Magic mismatch implies the whole EEPROM might be a different firmware's
  layout → reset everything.
- Calibration reset invalidates programData because charge profiles
  reference calibrated voltages / currents (e.g. battery.Vc_per_cell
  is meaningful only relative to a valid calibration).
- ProgramData reset invalidates settings because the global current /
  power limits depend on which battery profiles exist.

Sensible engineering. Could be a one-line `//` comment in source.

## Interactive recovery flow

On any failed verification, `Screen::runAskResetEeprom(what)` is
invoked. Looking at `Screen.cpp:206`:

```cpp
void Screen::runAskResetEeprom(uint8_t what)
{
    lcdClear();
    lcdSetCursor0_0();
    lcdPrint_P(PSTR("eeprom reset:"));
    lcdPrintUInt(what);
    lcdSetCursor0_1();
    lcdPrint_P(PSTR("            yes"));
    while (waitButtonPressed() != BUTTON_START);
}
```

The screen blocks **forever** waiting for `BUTTON_START`. There's no
timeout. **This is the single biggest concern with the EEPROM recovery
design.**

Implications:
- A charger in autonomous / unattended deployment will sit at this
  prompt indefinitely after any corruption.
- The "yes" label is hard-coded — there's no "no, do not reset" option.
  The only way out is to power-cycle (and the corruption will be
  re-detected on next boot, presenting the same prompt again).

After confirmation: `Screen::displayResettingEeprom()` (a brief toast),
then `testOrRestore(what)` with the restore bits set, which writes the
PROGMEM defaults via `AnalogInputs::restoreDefault()` /
`ProgramData::restoreDefault()` / `Settings::restoreDefault()`. Each
default-restorer is the per-module factory init that has been audited
elsewhere.

Finally `Screen::runResetEepromDone(before, after)` shows the result —
"please calibrate" if the reset was clean, or "eeprom reset error: N"
if any of the writes themselves failed verification (would indicate
EEPROM hardware failure, not just corruption).

## Where eeprom::check() is called

```
src/core/ChealiCharger2.cpp:66       eeprom::check();    // boot
src/core/calibration/Calibration.cpp:133  if(!eeprom::check())   // program-start
```

At every program start (via `Calibration::check()`), the EEPROM is
re-verified. Defensive — catches any EEPROM corruption that occurred
during runtime (extremely rare, but possible: cosmic-ray bit-flip in
EEPROM cells, or a write that didn't fully commit before power-off).

## Concerns / suggested follow-ups

| # | Severity | Item |
|---|---|---|
| α | low | Interactive prompt blocks unattended boot indefinitely. Consider a 60-second timeout that defaults to "yes, reset". |
| β | low | No "do not reset" / "abort" path. User who wants to retry power-cycle has no way to refuse the reset prompt. (Power-off works, but is awkward.) |
| γ | doc-only | The cascade rule in `restoreDefault` deserves a one-line comment explaining the why (magic → calibration → programData → settings dependency chain). |
| δ | very low | No backup/redundancy of the critical EEPROM regions. A single CRC error wipes the entire section. Standard practice in safety-critical embedded is dual-bank EEPROM. Probably overkill for a hobby charger, but worth noting. |
| ε | doc-only | The CRC polynomial (0xA001) is the Modbus / CRC-16/IBM-3740 variant. Worth a single-line `// CRC-16 IBM-3740 / Modbus` comment in `crc16_update`. |

None of these are pressing. The EEPROM-recovery system is **the
best-engineered subsystem we've audited so far** in this firmware,
and the audit's main contribution is documenting how it works for
future maintainers.

Closes the read-only part of #15. Issue stays open as the umbrella for
the α / β follow-ups (or for explicit close-with-no-action if you'd
rather accept the current behaviour).
