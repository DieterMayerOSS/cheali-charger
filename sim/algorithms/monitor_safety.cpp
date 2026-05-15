#include "monitor_safety.h"

namespace cheali_sim {

MonitorRunResult monitor_safety_check(const MonitorRunInputs& in)
{
    if (!in.on) {
        return {StrategyStatus::Running, MonitorStopReason::None};
    }

    // 1) Internal temperature (firmware: #ifdef ENABLE_T_INTERNAL)
    if (in.enable_t_internal) {
        if (in.t_internal >
            static_cast<uint16_t>(in.discharge_temp_off + in.temp_difference)) {
            return {StrategyStatus::Error,
                    MonitorStopReason::InternalTemperatureTooHigh};
        }
    }

    // 2) Battery presence via Vout_plus_pin ADC
    if (in.vmout_adc_max_limit <= in.vmout_adc ||
        (in.vmout_adc < in.vmout_adc_min_limit && in.discharger_is_power_on)) {
        return {StrategyStatus::Error,
                MonitorStopReason::BatteryDisconnected};
    }

    // 3) External-error flag set by another module
    if (in.external_error_battery_disconnected) {
        return {StrategyStatus::Error,
                MonitorStopReason::BatteryDisconnected};
    }

    // 4) Balance port disconnected mid-program
    if (in.initial_balance_port_connected !=
        in.current_balance_port_connected) {
        return {StrategyStatus::Error,
                MonitorStopReason::BalancePortDisconnected};
    }

    // 5) Overcurrent: maxI + 1A grace before tripping
    const uint16_t i_limit =
        static_cast<uint16_t>(in.i_max + in.i_limit_margin);
    if (i_limit < in.iout) {
        return {StrategyStatus::Error,
                MonitorStopReason::OutputCurrentTooHigh};
    }

    // 6) PSU sag
    if (in.vin < in.input_voltage_low) {
        return {StrategyStatus::Error,
                MonitorStopReason::InputVoltageTooLow};
    }

    // 7) Capacity-limit termination (Complete, not Error)
    if (in.capacity_limit != in.analog_max_charge &&
        in.capacity_limit <= in.cout) {
        return {StrategyStatus::Complete,
                MonitorStopReason::CapacityLimit};
    }

    // 8) Time-limit termination (Complete)
    if (in.enable_time_limit) {
        if (in.time_limit < in.analog_max_time_limit) {
            if (in.time_limit <= in.total_charge_time_min) {
                return {StrategyStatus::Complete,
                        MonitorStopReason::TimeLimit};
            }
        }
    }

    // 9) External-temperature cutoff (per-battery flag)
    if (in.enable_extern_t) {
        if (in.extern_tco < in.textern) {
            return {StrategyStatus::Error,
                    MonitorStopReason::ExternalTemperatureCutOff};
        }
    }

    return {StrategyStatus::Running, MonitorStopReason::None};
}

}  // namespace cheali_sim
