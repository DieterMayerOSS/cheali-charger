# STM32F030C8T6 pinout — "ROB" iMaxB6 80W board

Source: schematic transcribed 2026-05-17 from a board designed by
"ROB".

**The actual port** lives in a separate repository:
[**DieterMayerOSS/cheali-charger-stm32**](https://github.com/DieterMayerOSS/cheali-charger-stm32) —
STM32CubeIDE-based, uses ST HAL/LL, embeds this fork's algorithmic
core. This document stays here as a reference so anyone else planning
an STM32 port of cheali-charger has a starting-point pin table.

## MCU

- **STM32F030C8T6** (LQFP-48)
- Cortex-M0 @ 48 MHz (8 MHz HSE via PLL)
- 64 KB Flash, 8 KB SRAM
- Powered from +3.3V (VDD @ pins 24/48, VDDA @ pin 9)

## Peripherals

### Power supply / reset

| Pin | Signal | Notes |
|---|---|---|
| 24, 48 | VDD | +3.3V rail, decoupled with 100 nF |
| 9 | VDDA | +3.3V analog rail via 10 R (R16) + 100 nF (C20) |
| 23, 47, 8 | VSS / VSSA | GND |
| 7 | NRST | 10 kΩ pull-up (R17), 100 nF cap (C22), reset button S5 |
| 44 | BOOT0 | 10 kΩ pull-down (R18) — boots from Flash |

### Crystal

| Pin | Signal | Notes |
|---|---|---|
| 5 | PF0 / OSC_IN | 8 MHz crystal Q1, 18 pF loads |
| 6 | PF1 / OSC_OUT | (same crystal, other side) |
| 3, 4 | PC14 / PC15 | Not used as LSE — repurposed as B5 / B6 balance sense |

### SWD programming

| Pin | Signal | Notes |
|---|---|---|
| 34 | PA13 / SWDIO | J4 = TC2030 Tag-Connect header, CN3 = ST-Link/V2 pinout |
| 37 | PA14 / SWDCLK | (same connectors) |

`CN3` reference pinout (from schematic annotation):
- pin 1 of 2: VAPP / Target (+3.3V)
- pin 7: SWDIO
- pin 9: SWDCLK
- pin 15: NRST
- pins 4, 6, 8, 10, 20: GND

## GPIO / peripheral assignment

### Analog inputs (ADC channels)

| Pin | Signal | Function |
|---|---|---|
| PA0 (10) | B+ | Battery pack positive terminal |
| PA1 (11) | B- | Battery pack negative terminal |
| PA2 (12) | I_DISCHARGE | Discharge current sense |
| PA3 (13) | I_CHARGE | Charge current sense |
| PA4 (14) | VIN | Input supply voltage |
| PA6 (16) | BALANCE-OUT | 8:1 mux output — cell voltages come back here |

### Control outputs (safety / relay drive)

| Pin | Signal | Function |
|---|---|---|
| PA5 (15) | DISCONNECT-BATT- | Isolate battery negative rail |
| PA7 (17) | DISABLE-DISCHARGE | Kill the discharge FET |
| PA8 (29) | DRIVER-DISABLE | Kill the main SMPS driver (safety) |
| PB4 (40) | BUZZER | Piezo driver |

### SMPS PWM

| Pin | Signal | Function |
|---|---|---|
| PB8 (45) | PWM_BUCK | Buck-stage PWM |
| PB9 (46) | PWM_BOOST | Boost-stage PWM |

**Note:** two independent PWM channels indicate a **buck-boost**
topology, unlike most existing cheali-charger targets which are
buck-only. This affects `SMPS.cpp` — the HAL needs to pick which
channel is active based on the strategy's needs, or drive both.

### Balancer port mux

The board uses a 3-bit-controlled 8-channel analog mux (74HC4051 or
equivalent) to route one of six cell-voltage taps into ADC channel
PA6.

| Pin | Signal | Function |
|---|---|---|
| PB5 (41) | S0 | Mux select bit 0 |
| PB6 (42) | S1 | Mux select bit 1 |
| PB7 (43) | S2 | Mux select bit 2 |
| PB13 (26) | B1 | Cell 1 tap (goes to mux input) |
| PB14 (27) | B2 | Cell 2 tap |
| PB15 (28) | B3 | Cell 3 tap |
| PC13 (2) | B4 | Cell 4 tap |
| PC14 (3) | B5 | Cell 5 tap |
| PC15 (4) | B6 | Cell 6 tap |

### Keyboard (four discrete buttons — not ADC-encoded!)

| Pin | Signal | Function |
|---|---|---|
| PB0 (18) | STOP | Stop / cancel key |
| PB1 (19) | INC+ | Increment |
| PB2 (20) | INC- | Decrement |
| PB3 (39) | ENTER | Enter / start |

This is a notable simplification vs. the AVR targets, which encode
all four buttons on a single ADC channel via a voltage divider.

### LCD (HD44780, 4-bit parallel)

| Pin | Signal | Function |
|---|---|---|
| PB10 (21) | DB4 | Data bit 4 |
| PA11 (32) | DB7 | Data bit 7 |
| PA12 (33) | DB6 | Data bit 6 |
| PA15 (38) | DB5 | Data bit 5 |
| PB11 (22) | E | Enable strobe |
| PB12 (25) | RS | Register select |

No R/W pin on the schematic — assumed tied to GND (write-only mode,
standard cheali-charger convention).

### USART1 (serial log / HC-05 / HM-18 header)

| Pin | Signal | Function |
|---|---|---|
| PA9 (30) | USART1_TX | TX out |
| PA10 (31) | USART1_RX | RX in |

Direct pin header on the PCB — no reverse-engineering / hardware mod
needed as on the AVR-based Turnigy MEGA 400Wx2.
