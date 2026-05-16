/*
    cheali-charger - open source firmware for a variety of LiPo chargers
    Copyright (C) 2013  Paweł Stawicki. All right reserved.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef HARDWARE_CONFIG_H_
#define HARDWARE_CONFIG_H_

#include "GlobalConfig.h"
#include "HardwareConfigGeneric.h"
#include "GTPowerA6-10-pins.h"

// Turnigy MEGA 400Wx2 has no exposed serial port (no TXD/RXD pin
// header on the PCB), so the SerialLog facility writes to a wire
// nobody listens to. Disable it to reclaim ~280 B SRAM (tx_buffer
// + Serial0) and ~1-3 KB of flash.
#undef ENABLE_SERIAL_LOG

#define MAX_CHARGE_V            ANALOG_VOLT(27.000)
#define MAX_CHARGE_I            ANALOG_AMP(20.000)
#define MAX_CHARGE_P            ANALOG_WATT(400.000)

#define MAX_DISCHARGE_P         ANALOG_WATT(25.000)
#define MAX_DISCHARGE_I         ANALOG_AMP(5.000)


// Half-resolution PWM duty for the 400W class: doubles the PWM frequency
// vs. TIMER1_PRECISION_PERIOD (used by 50W/200W targets), at the cost of
// halving the duty-cycle granularity. The higher switching frequency
// keeps the SMPS inductor / output ripple within spec for the larger
// power envelope.
#define SMPS_UPPERBOUND_VALUE               (TIMER1_PRECISION_PERIOD/2)
#define DISCHARGER_UPPERBOUND_VALUE         TIMER1_PRECISION_PERIOD
#define ADC_KEY_BORDER 128

#endif /* HARDWARE_CONFIG_H_ */
