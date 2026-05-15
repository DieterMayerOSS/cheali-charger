#pragma once

// Pure-logic port of Monitor::run() from src/core/strategy/Monitor.cpp.
//
// The firmware's run() function is called every cycle and decides whether
// to stop the program for safety / completion reasons. It mixes nine
// different checks in a strict order (first hit wins) and writes a
// human-readable reason string into Program::stopReason as a side effect.
//
// We port the decision as a pure function: take a snapshot of all
// inputs, return a status + a typed reason code.

#include <cstdint>

#include "charge_strategy_decisions.h"  // for StrategyStatus

namespace cheali_sim {

// Typed reason for stopping. None == still running.
enum class MonitorStopReason : uint8_t {
    None = 0,
    InternalTemperatureTooHigh,
    BatteryDisconnected,
    BalancePortDisconnected,
    OutputCurrentTooHigh,
    InputVoltageTooLow,
    CapacityLimit,
    TimeLimit,
    ExternalTemperatureCutOff,
};

// Snapshot of every value Monitor::run() consults. Field names track the
// firmware identifiers; the comments map each to its origin.
struct MonitorRunInputs {
    // Whether monitoring is active (Monitor::on_). When false, run()
    // returns Running immediately without checking anything else.
    bool     on;

    // --- Internal temperature (ENABLE_T_INTERNAL conditional in firmware) ---
    bool     enable_t_internal;           // compile-time #ifdef -> runtime flag
    uint16_t t_internal;                  // AnalogInputs::Tintern
    uint16_t discharge_temp_off;          // settings.dischargeTempOff
    uint16_t temp_difference;             // Settings::TempDifference

    // --- Battery presence (Vout_plus ADC reading) ---
    uint16_t vmout_adc;                   // ADC value of Vout_plus_pin
    uint16_t vmout_adc_min_limit;         // Vout_plus_adcMinLimit_
    uint16_t vmout_adc_max_limit;         // Vout_plus_adcMaxLimit_
    bool     discharger_is_power_on;      // Discharger::isPowerOn()

    // --- External error flag (set by ISR / other module) ---
    bool     external_error_battery_disconnected;
                                          // i_externalError == BATTERY_DISCONNECTED

    // --- Balance port connection sanity ---
    bool     initial_balance_port_connected;
                                          // captured at powerOn()
    bool     current_balance_port_connected;
                                          // AnalogInputs::isBalancePortConnected()

    // --- Output current overcurrent ---
    uint16_t iout;                        // AnalogInputs::getIout()
    uint16_t i_max;                       // Strategy::maxI
    uint16_t i_limit_margin;              // ANALOG_AMP(1.000) = 1000 (mA)

    // --- Input undervoltage (PSU sag) ---
    uint16_t vin;                         // AnalogInputs::Vin
    uint16_t input_voltage_low;           // settings.inputVoltageLow

    // --- Capacity-limit termination ---
    uint16_t cout;                        // AnalogInputs::Cout
    uint16_t capacity_limit;              // ProgramData::getCapacityLimit()
    uint16_t analog_max_charge;           // sentinel meaning "unlimited"

    // --- Time-limit termination (ENABLE_TIME_LIMIT in firmware) ---
    bool     enable_time_limit;
    uint16_t total_charge_time_min;       // getTotalChargeDischargeTimeMin()
    uint16_t time_limit;                  // ProgramData::getTimeLimit()
    uint16_t analog_max_time_limit;       // sentinel "unlimited"

    // --- External temperature cutoff (per-battery flag) ---
    bool     enable_extern_t;             // ProgramData::battery.enable_externT
    uint16_t textern;                     // AnalogInputs::Textern
    uint16_t extern_tco;                  // ProgramData::battery.externTCO
};

struct MonitorRunResult {
    StrategyStatus     status;
    MonitorStopReason  reason;
};

// Verbatim port of Monitor::run(). Checks fire in the firmware's
// declared order: the first matching condition wins, later checks
// are not evaluated.
MonitorRunResult monitor_safety_check(const MonitorRunInputs& in);

}  // namespace cheali_sim
