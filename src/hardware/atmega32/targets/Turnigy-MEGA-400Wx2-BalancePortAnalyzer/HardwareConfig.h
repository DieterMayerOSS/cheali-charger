/*
    cheali-charger - open source firmware for a variety of LiPo chargers

    Balance-port diagnostic build for the Turnigy MEGA 400Wx2 charger.
    Uses the exact same hardware pinout, power limits, and factory
    calibration defaults as the normal Turnigy-MEGA-400Wx2 firmware,
    but boots into BalancePortAnalyzer (helper mode) instead of the
    main menu. Flash this when you want to verify the balance-port
    pin ADC readings on real hardware without driving the SMPS.

    Switch back to the normal firmware by re-flashing
    Turnigy-MEGA-400Wx2_atmega32.hex.
*/
#ifndef HARDWARE_CONFIG_H_
#define HARDWARE_CONFIG_H_

#include "GlobalConfig.h"
#include "HardwareConfigGeneric.h"
#include "GTPowerA6-10-pins.h"

#define ENABLE_HELPER
#define ENABLE_HELPER_BALANCE_PORT_ANALYZER

#define MAX_CHARGE_V            ANALOG_VOLT(27.000)
#define MAX_CHARGE_I            ANALOG_AMP(20.000)
#define MAX_CHARGE_P            ANALOG_WATT(400.000)

#define MAX_DISCHARGE_P         ANALOG_WATT(25.000)
#define MAX_DISCHARGE_I         ANALOG_AMP(5.000)

#define SMPS_UPPERBOUND_VALUE               (TIMER1_PRECISION_PERIOD/2)
#define DISCHARGER_UPPERBOUND_VALUE         TIMER1_PRECISION_PERIOD
#define ADC_KEY_BORDER 128

#endif /* HARDWARE_CONFIG_H_ */
