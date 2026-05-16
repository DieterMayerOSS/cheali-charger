# docs/ — engineering notes for this fork

This directory mixes upstream documentation (charger-specific photos,
LogView INI files, etc.) with engineering audits / design notes added
in this fork. The audit documents are the result of code-review
passes recorded as deliverables for GitHub issues.

## Audit documents

| Doc | Subject | Source issue | TL;DR |
|---|---|---|---|
| [smps_audit.md](smps_audit.md) | `src/core/strategy/SMPS.cpp` — the PWM / setpoint controller for the 200 W / 400 W output | [#13](https://github.com/DieterMayerOSS/cheali-charger/issues/13) | Small (130 LOC), deterministic, but several undocumented design choices. Seven findings, none safety-critical. |
| [watchdog_audit.md](watchdog_audit.md) | Whether an AVR watchdog timer is set up and kicked | [#14](https://github.com/DieterMayerOSS/cheali-charger/issues/14) | **No watchdog at all.** Zero hits for `<avr/wdt.h>` / `wdt_enable` / `wdt_reset` across the codebase. Defensive-engineering gap. |
| [eeprom_recovery.md](eeprom_recovery.md) | `src/core/eeprom.cpp` — corruption detection and recovery flow | [#15](https://github.com/DieterMayerOSS/cheali-charger/issues/15) | Surprisingly well-engineered. Four-tier verification (magic + architecture + version + CRC-16/Modbus), interactive recovery with read-retry. Only minor concerns (interactive prompt has no timeout — fixed in commit f4c651a2). |

Audit documents are read-only deliverables: they document findings but
don't change code. Concrete code changes that follow from an audit are
spun off as separate issues or done inline if trivial.

## Upstream / hardware documentation

The other subdirectories (`GTPowerA6-10/`, `Turnigy_MEGA400x2/`,
`Turnigy MAX80/`, etc.) carry photos, schematics, and target-specific
notes inherited from upstream. They predate this fork's audit pass.
