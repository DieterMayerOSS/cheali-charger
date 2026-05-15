#include "monitor_safety.h"

#include <cassert>
#include <cstdio>

using cheali_sim::StrategyStatus;
using cheali_sim::MonitorStopReason;
using cheali_sim::MonitorRunInputs;
using cheali_sim::MonitorRunResult;
using cheali_sim::monitor_safety_check;

// Default: everything healthy, monitoring on, all flags configured to
// pass all checks. Each test then perturbs ONE field to trip a check.
static MonitorRunInputs make_healthy()
{
    MonitorRunInputs in{};
    in.on = true;

    in.enable_t_internal       = true;
    in.t_internal              = 25;
    in.discharge_temp_off      = 60;
    in.temp_difference         = 5;

    in.vmout_adc               = 5000;
    in.vmout_adc_min_limit     = 1000;
    in.vmout_adc_max_limit     = 10000;
    in.discharger_is_power_on  = false;

    in.external_error_battery_disconnected = false;

    in.initial_balance_port_connected = true;
    in.current_balance_port_connected = true;

    in.iout            = 500;
    in.i_max           = 1000;
    in.i_limit_margin  = 1000;

    in.vin             = 12000;
    in.input_voltage_low = 10000;

    in.cout            = 100;
    in.capacity_limit  = 2000;
    in.analog_max_charge = 0xFFFF;  // sentinel "unlimited"

    in.enable_time_limit      = true;
    in.total_charge_time_min  = 10;
    in.time_limit             = 120;
    in.analog_max_time_limit  = 0xFFFF;

    in.enable_extern_t = false;
    in.textern         = 0;
    in.extern_tco      = 60;
    return in;
}

static void test_healthy_returns_running()
{
    auto r = monitor_safety_check(make_healthy());
    assert(r.status == StrategyStatus::Running);
    assert(r.reason == MonitorStopReason::None);
}

static void test_off_short_circuits_to_running()
{
    auto in = make_healthy();
    in.on = false;
    // Even with multiple "errors" set, on==false should return Running
    in.external_error_battery_disconnected = true;
    in.t_internal = 99;
    auto r = monitor_safety_check(in);
    assert(r.status == StrategyStatus::Running);
    assert(r.reason == MonitorStopReason::None);
}

static void test_internal_temp_too_high()
{
    auto in = make_healthy();
    // discharge_temp_off + temp_difference = 65; strict > only
    in.t_internal = 66;
    auto r = monitor_safety_check(in);
    assert(r.status == StrategyStatus::Error);
    assert(r.reason == MonitorStopReason::InternalTemperatureTooHigh);
}

static void test_internal_temp_at_limit_ok()
{
    auto in = make_healthy();
    in.t_internal = 65;  // exactly at threshold, uses >
    auto r = monitor_safety_check(in);
    assert(r.status == StrategyStatus::Running);
}

static void test_internal_temp_check_skipped_when_disabled()
{
    auto in = make_healthy();
    in.enable_t_internal = false;
    in.t_internal = 200;  // huge, but check is off
    auto r = monitor_safety_check(in);
    assert(r.status == StrategyStatus::Running);
}

static void test_vmout_above_max_is_battery_disconnected()
{
    auto in = make_healthy();
    in.vmout_adc = in.vmout_adc_max_limit;  // <=, so equal trips
    auto r = monitor_safety_check(in);
    assert(r.status == StrategyStatus::Error);
    assert(r.reason == MonitorStopReason::BatteryDisconnected);
}

static void test_vmout_below_min_only_trips_when_discharging()
{
    auto in = make_healthy();
    in.vmout_adc = in.vmout_adc_min_limit - 1;

    // Discharger off -> no error (sag while charging is fine, the
    // charger might just be off)
    in.discharger_is_power_on = false;
    assert(monitor_safety_check(in).status == StrategyStatus::Running);

    // Discharger on -> error (load present but no battery voltage)
    in.discharger_is_power_on = true;
    auto r = monitor_safety_check(in);
    assert(r.status == StrategyStatus::Error);
    assert(r.reason == MonitorStopReason::BatteryDisconnected);
}

static void test_external_error_flag()
{
    auto in = make_healthy();
    in.external_error_battery_disconnected = true;
    auto r = monitor_safety_check(in);
    assert(r.status == StrategyStatus::Error);
    assert(r.reason == MonitorStopReason::BatteryDisconnected);
}

static void test_balance_port_disconnected()
{
    auto in = make_healthy();
    in.initial_balance_port_connected = true;
    in.current_balance_port_connected = false;
    auto r = monitor_safety_check(in);
    assert(r.status == StrategyStatus::Error);
    assert(r.reason == MonitorStopReason::BalancePortDisconnected);

    // Symmetric: was unplugged, now plugged in mid-program also fails
    in.initial_balance_port_connected = false;
    in.current_balance_port_connected = true;
    r = monitor_safety_check(in);
    assert(r.status == StrategyStatus::Error);
    assert(r.reason == MonitorStopReason::BalancePortDisconnected);
}

static void test_output_current_overcurrent()
{
    auto in = make_healthy();
    // i_limit = i_max + i_limit_margin = 1000 + 1000 = 2000
    // Firmware uses `i_limit < iout` (strict)
    in.iout = 2001;
    auto r = monitor_safety_check(in);
    assert(r.status == StrategyStatus::Error);
    assert(r.reason == MonitorStopReason::OutputCurrentTooHigh);
}

static void test_output_current_exactly_at_limit_ok()
{
    auto in = make_healthy();
    in.iout = 2000;  // i_max + margin, strict < required
    auto r = monitor_safety_check(in);
    assert(r.status == StrategyStatus::Running);
}

static void test_input_voltage_too_low()
{
    auto in = make_healthy();
    in.vin = in.input_voltage_low - 1;
    auto r = monitor_safety_check(in);
    assert(r.status == StrategyStatus::Error);
    assert(r.reason == MonitorStopReason::InputVoltageTooLow);
}

static void test_capacity_limit_reached_is_complete()
{
    auto in = make_healthy();
    in.cout = in.capacity_limit;  // uses <=, equality trips
    auto r = monitor_safety_check(in);
    assert(r.status == StrategyStatus::Complete);
    assert(r.reason == MonitorStopReason::CapacityLimit);
}

static void test_capacity_unlimited_never_trips()
{
    auto in = make_healthy();
    in.capacity_limit = in.analog_max_charge;  // sentinel "unlimited"
    in.cout = 0xFFFE;  // huge, but check skipped because limit == sentinel
    auto r = monitor_safety_check(in);
    assert(r.status == StrategyStatus::Running);
}

static void test_time_limit_reached_is_complete()
{
    auto in = make_healthy();
    in.total_charge_time_min = in.time_limit;  // uses <=
    auto r = monitor_safety_check(in);
    assert(r.status == StrategyStatus::Complete);
    assert(r.reason == MonitorStopReason::TimeLimit);
}

static void test_time_limit_disabled()
{
    auto in = make_healthy();
    in.enable_time_limit = false;
    in.total_charge_time_min = 60000;  // far above time_limit, but check off
    auto r = monitor_safety_check(in);
    assert(r.status == StrategyStatus::Running);
}

static void test_external_temperature_cutoff()
{
    auto in = make_healthy();
    in.enable_extern_t = true;
    // strict < : firmware writes `externTCO < Textern`
    in.textern = in.extern_tco + 1;
    auto r = monitor_safety_check(in);
    assert(r.status == StrategyStatus::Error);
    assert(r.reason == MonitorStopReason::ExternalTemperatureCutOff);
}

static void test_external_temperature_disabled()
{
    auto in = make_healthy();
    in.enable_extern_t = false;
    in.textern = 999;  // huge, but check disabled
    auto r = monitor_safety_check(in);
    assert(r.status == StrategyStatus::Running);
}

static void test_order_first_match_wins()
{
    // Multiple conditions set. Internal temp is checked first
    // (after the on==true gate), so it should win.
    auto in = make_healthy();
    in.t_internal = 200;                       // would trigger internal-temp
    in.external_error_battery_disconnected = true;  // would trigger battery-disc
    in.cout = in.capacity_limit;               // would trigger capacity-limit
    auto r = monitor_safety_check(in);
    assert(r.status == StrategyStatus::Error);
    assert(r.reason == MonitorStopReason::InternalTemperatureTooHigh);
}

int main()
{
    test_healthy_returns_running();
    test_off_short_circuits_to_running();
    test_internal_temp_too_high();
    test_internal_temp_at_limit_ok();
    test_internal_temp_check_skipped_when_disabled();
    test_vmout_above_max_is_battery_disconnected();
    test_vmout_below_min_only_trips_when_discharging();
    test_external_error_flag();
    test_balance_port_disconnected();
    test_output_current_overcurrent();
    test_output_current_exactly_at_limit_ok();
    test_input_voltage_too_low();
    test_capacity_limit_reached_is_complete();
    test_capacity_unlimited_never_trips();
    test_time_limit_reached_is_complete();
    test_time_limit_disabled();
    test_external_temperature_cutoff();
    test_external_temperature_disabled();
    test_order_first_match_wins();

    std::puts("test_monitor_safety: OK (19 cases)");
    return 0;
}
