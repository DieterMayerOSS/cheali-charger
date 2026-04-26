Pb / AGM Float Charge
=====================

The **float charge** program is available for **Pb** and **AGM** battery types.  
It is designed for long-term maintenance charging — for example, keeping a motorbike  
or car battery topped up while the vehicle is stored.

How it works
------------

Float charge runs an auto-restarting CCCV (Constant Current / Constant Voltage) cycle  
targeting the float voltage instead of the full charge voltage:

1. **CC phase** — charges at the battery's configured `Ic` until the float voltage  
   is reached. The display shows `CC` on the status screen.
2. **CV phase** — the Thévenin algorithm tapers the current down to exactly what is  
   needed to maintain the float voltage. The display shows `CV`.
3. **Wait phase** — if the current drops below `min Ic` (e.g. because the parasitic  
   load momentarily fell below that threshold), the charger stops output and waits  
   for `DC Rest Time` minutes. The display shows `W`. After the wait, the CC/CV  
   cycle restarts automatically.

The program runs **indefinitely** and only exits when the user presses **STOP**  
or a monitor error occurs (temperature, time limit, etc.).

Voltages
--------

| Battery type | Charge voltage  | Float voltage   |
|:-------------|----------------:|----------------:|
| Pb           | 2.450 V/cell    | 2.250 V/cell    |
| AGM          | 2.300 V/cell    | 2.275 V/cell    |

For a standard 12V (6-cell) AGM battery:
- Charges up to **13.80 V**, then holds at **13.65 V**.

The float voltage (`Vs/cell`) is adjustable per battery in  
"edit battery" → advanced settings → `Vs/cell`.

Restart interval
----------------

The wait time between auto-restarts is controlled by the **DC Rest Time** setting,  
visible in "edit battery" → advanced settings → `DC rest time` (1–99 minutes, default 30).

Increase it to reduce charge frequency; decrease it to restart sooner after a  
load spike drops the current below `min Ic`.

Status screen
-------------

During float charge, a dedicated status screen is available (press `Inc`/`Dec` to cycle to it):

![status screen](status_screen_pb_float.png) 

**During charging (CC or CV phase):**

| Field         | Description                                              |
|:--------------|:---------------------------------------------------------|
| `CC` / `CV`   | Current phase: CC = charging up, CV = maintaining float  |
| `0.108A`      | Current being delivered by the charger                   |
| `13.621V`     | Live battery voltage                                     |
| `→13.650V`    | Target float voltage                                     |

**During wait phase:**

| Field         | Description                                              |
|:--------------|:---------------------------------------------------------|
| `W`           | Waiting before restarting float charge                   |
| `00:12:34`    | Time elapsed since the wait started                      |
| `13.521V`     | Battery resting voltage (charger output off)             |

Battery setup
-------------

Example setup for a 12V motorbike AGM battery:

| Setting         | Value                                        |
|:----------------|:---------------------------------------------|
| Battery type    | `AGM`                                        |
| Cells (`V:`)    | `6` (12V battery)                            |
| Capacity        | actual Ah rating of the battery              |
| Ic              | ~C/10 of the battery, e.g. `1000mA` for 10Ah |
| DC Rest Time    | restart interval after low-current stop (default 30 min) |
| Program         | `float charge`                               |

Limitations
-----------

- **Constant parasitic loads** (alarm, GPS, clock) present when the CV phase starts are  
  compensated automatically — the algorithm maintains enough current to cover them.
- **Variable parasitic loads** that occasionally drop below `min Ic` will cause the  
  charger to enter the wait phase and restart. Reduce `min Ic` or `DC Rest Time`  
  to tune this behaviour.
- **Large intermittent loads** (e.g. starter motor, headlights) are **not** actively  
  compensated. The charger holds its current fixed; the battery buffers the difference  
  and voltage drops until the load is removed.
- If the battery voltage drops significantly while a large load is applied, the charger  
  does **not** increase current — this is by design in the Thévenin CV algorithm, which  
  only allows current to decrease in CV phase to avoid overcharging.

Calibration note
----------------

If you see **calib. error I charge 14** when starting float charge, decrease `max Ic`  
in "settings". The default for Dual-Power-B6AC-80W-RC is 5.0A, which is safe.  
See [calibration error codes](calibration_error_codes.md) for details.
