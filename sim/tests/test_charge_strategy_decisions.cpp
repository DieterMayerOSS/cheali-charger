#include "charge_strategy_decisions.h"

#include <cassert>
#include <cstdio>

using cheali_sim::StrategyStatus;
using cheali_sim::simple_charge_decide;
using cheali_sim::DeltaChargeInputs;
using cheali_sim::DeltaChargeOutputs;
using cheali_sim::delta_charge_decide;
using cheali_sim::thevenin_is_end_vout;

// ---- simple_charge_decide -------------------------------------------------

static void test_simple_charge_below_limit()
{
    assert(simple_charge_decide(4100, 4200) == StrategyStatus::Running);
}

static void test_simple_charge_at_limit_is_still_running()
{
    // Strict > only: equality returns Running (charge keeps going)
    assert(simple_charge_decide(4200, 4200) == StrategyStatus::Running);
}

static void test_simple_charge_overvoltage_is_error()
{
    // SimpleCharge treats overvoltage as Error, not Complete —
    // this is the safety stop, not a normal terminus.
    assert(simple_charge_decide(4201, 4200) == StrategyStatus::Error);
}

// ---- delta_charge_decide --------------------------------------------------

// Build a sane baseline: charging NiMH, no triggers fire, status Running.
static DeltaChargeInputs make_default_delta_inputs()
{
    DeltaChargeInputs in{};
    in.v_battery            = 1300;
    in.v_end                = 1500;
    in.v_discharged         = 1000;
    in.delta_count          = 10;
    in.delta_textern        = 0;
    in.delta_vout           = 0;
    in.delta_t_limit        = 100;
    in.delta_v_limit        = -5;
    in.delta_v_ignore_min   = 5;     // ignore first 5 minutes
    in.delta_counts_per_min = 2;     // so ignore until count >= 10
    in.enable_extern_t      = false;
    in.enable_delta_v       = true;
    return in;
}

static void test_delta_baseline_running()
{
    auto in = make_default_delta_inputs();
    auto out = delta_charge_decide(in);
    assert(out.status == StrategyStatus::Running);
    // v_battery (1300) > v_discharged (1000) -> kick up current
    assert(out.set_iout_to_max == true);
    // count == 10, threshold == 5*2 == 10 -> past_ignore_window true
    assert(out.enable_delta_vout_max == true);
}

static void test_delta_below_discharged_no_kickup()
{
    auto in = make_default_delta_inputs();
    in.v_battery = 900;  // below v_discharged
    auto out = delta_charge_decide(in);
    assert(out.set_iout_to_max == false);
}

static void test_delta_overvoltage_completes()
{
    // Note: delta-charge treats overvoltage as Complete (normal terminus
    // for NiMH/NiCd), not Error like SimpleCharge does.
    auto in = make_default_delta_inputs();
    in.v_battery = in.v_end + 1;
    auto out = delta_charge_decide(in);
    assert(out.status == StrategyStatus::Complete);
}

static void test_delta_count_below_2_returns_running()
{
    auto in = make_default_delta_inputs();
    in.delta_count = 1;
    auto out = delta_charge_decide(in);
    assert(out.status == StrategyStatus::Running);
    // Side effects from earlier in the function still apply: kickup
    // is decided before the count check.
    assert(out.set_iout_to_max == true);
    // But the past-ignore-window flag should be false (we never reach it)
    assert(out.enable_delta_vout_max == false);
}

static void test_delta_external_temperature_completes()
{
    auto in = make_default_delta_inputs();
    in.enable_extern_t = true;
    in.delta_textern   = in.delta_t_limit + 1;  // overshoot
    auto out = delta_charge_decide(in);
    assert(out.status == StrategyStatus::Complete);
}

static void test_delta_external_temperature_disabled_ignored()
{
    auto in = make_default_delta_inputs();
    in.enable_extern_t = false;
    in.delta_textern   = 9999;  // huge, but flag disabled
    auto out = delta_charge_decide(in);
    assert(out.status == StrategyStatus::Running);
}

static void test_delta_ignore_window_blocks_dv_check()
{
    auto in = make_default_delta_inputs();
    in.delta_count = 5;  // below 10 = threshold
    in.delta_vout  = -100;  // way below trigger
    auto out = delta_charge_decide(in);
    // -dV would trigger Complete, but we're still in the ignore window
    assert(out.status == StrategyStatus::Running);
    assert(out.enable_delta_vout_max == false);
}

static void test_delta_negative_dv_after_ignore_window_completes()
{
    auto in = make_default_delta_inputs();
    in.delta_count = 10;  // == threshold (5 min * 2 per min)
    in.delta_vout  = in.delta_v_limit - 1;  // strictly below trigger
    auto out = delta_charge_decide(in);
    assert(out.status == StrategyStatus::Complete);
}

static void test_delta_dv_disabled_no_termination()
{
    auto in = make_default_delta_inputs();
    in.enable_delta_v = false;
    in.delta_count    = 20;
    in.delta_vout     = -1000;  // huge -dV, but check disabled
    auto out = delta_charge_decide(in);
    assert(out.status == StrategyStatus::Running);
    // enable_delta_vout_max still set (firmware sets it independent of flag)
    assert(out.enable_delta_vout_max == true);
}

// ---- thevenin_is_end_vout -------------------------------------------------

static void test_thevenin_end_any_cell_at_limit()
{
    // Any cell at its per-cell limit -> end, regardless of balancer state
    assert(thevenin_is_end_vout(/*Vbat=*/3700, /*Vend=*/4200,
                                /*any_cell_at_limit=*/true,
                                /*balancer_working=*/true) == true);
    assert(thevenin_is_end_vout(3700, 4200, true, false) == true);
}

static void test_thevenin_end_balancer_working_pack_voltage_ignored()
{
    // Balancer running: per-cell readings drift, so the pack-voltage
    // fallback is suppressed. Even if Vbat exceeds Vend, return false
    // unless a cell is at its limit.
    assert(thevenin_is_end_vout(/*Vbat=*/4300, /*Vend=*/4200,
                                /*any_cell_at_limit=*/false,
                                /*balancer_working=*/true) == false);
}

static void test_thevenin_end_balancer_off_pack_voltage_at_limit()
{
    // Balancer idle: pack voltage at/above end voltage triggers end.
    assert(thevenin_is_end_vout(4200, 4200, false, false) == true);
    assert(thevenin_is_end_vout(4300, 4200, false, false) == true);
}

static void test_thevenin_end_balancer_off_below_limit()
{
    // Balancer idle, pack below limit, no cell at limit -> not done.
    assert(thevenin_is_end_vout(4100, 4200, false, false) == false);
}

int main()
{
    test_simple_charge_below_limit();
    test_simple_charge_at_limit_is_still_running();
    test_simple_charge_overvoltage_is_error();

    test_delta_baseline_running();
    test_delta_below_discharged_no_kickup();
    test_delta_overvoltage_completes();
    test_delta_count_below_2_returns_running();
    test_delta_external_temperature_completes();
    test_delta_external_temperature_disabled_ignored();
    test_delta_ignore_window_blocks_dv_check();
    test_delta_negative_dv_after_ignore_window_completes();
    test_delta_dv_disabled_no_termination();

    test_thevenin_end_any_cell_at_limit();
    test_thevenin_end_balancer_working_pack_voltage_ignored();
    test_thevenin_end_balancer_off_pack_voltage_at_limit();
    test_thevenin_end_balancer_off_below_limit();

    std::puts("test_charge_strategy_decisions: OK (16 cases)");
    return 0;
}
