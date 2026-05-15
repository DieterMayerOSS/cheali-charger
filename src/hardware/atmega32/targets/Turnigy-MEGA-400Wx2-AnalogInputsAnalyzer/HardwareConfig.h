/*
    cheali-charger - open source firmware for a variety of LiPo chargers

    Full-ADC diagnostic build for the Turnigy MEGA 400Wx2 charger.
    Boots into AnalogInputsAnalyzer (helper mode) which displays every
    physical ADC channel — voltage rails, currents, internal/external
    temperature, balance-port pins — without driving the SMPS. Useful
    for one-shot hardware sanity checks before trusting the regular
    firmware on a battery.

    Switch back to the normal firmware by re-flashing
    Turnigy-MEGA-400Wx2_atmega32.hex.
*/
#ifndef HARDWARE_CONFIG_H_
#define HARDWARE_CONFIG_H_

#include "GlobalConfig.h"
#include "HardwareConfigGeneric.h"
#include "GTPowerA6-10-pins.h"

// Same hardware as the main Mega 400Wx2 build — no serial port.
#undef ENABLE_SERIAL_LOG

#define ENABLE_HELPER
#define ENABLE_HELPER_ANALOG_INPUTS_ANALYZER

#define MAX_CHARGE_V            ANALOG_VOLT(27.000)
#define MAX_CHARGE_I            ANALOG_AMP(20.000)
#define MAX_CHARGE_P            ANALOG_WATT(400.000)

#define MAX_DISCHARGE_P         ANALOG_WATT(25.000)
#define MAX_DISCHARGE_I         ANALOG_AMP(5.000)

#define SMPS_UPPERBOUND_VALUE               (TIMER1_PRECISION_PERIOD/2)
#define DISCHARGER_UPPERBOUND_VALUE         TIMER1_PRECISION_PERIOD
#define ADC_KEY_BORDER 128

#endif /* HARDWARE_CONFIG_H_ */
