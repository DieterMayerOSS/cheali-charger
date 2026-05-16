#include "monitor_eta.h"

#include <cassert>
#include <climits>
#include <cstdio>

using cheali_sim::compute_charge_percent;
using cheali_sim::EtaState;
using cheali_sim::update_eta_state;
using cheali_sim::compute_eta_time;

// ----- compute_charge_percent ---------------------------------------------

static void test_percent_below_empty_returns_zero()
{
    // v < v_empty -> 0
    assert(compute_charge_percent(2500, 3000, 4200) == 0);
    // v == v_empty -> 0 (boundary uses <=)
    assert(compute_charge_percent(3000, 3000, 4200) == 0);
}

static void test_percent_at_or_above_charged_returns_99()
{
    // Firmware never returns 100 — that's reserved for "complete"
    assert(compute_charge_percent(4200, 3000, 4200) == 99);
    assert(compute_charge_percent(4300, 3000, 4200) == 99);
}

static void test_percent_midpoint()
{
    // 3.0V..4.2V span = 1200 mV, /100 = 12. Midpoint v=3600 -> 600/12 = 50%
    assert(compute_charge_percent(3600, 3000, 4200) == 50);
}

static void test_percent_just_above_empty_truncates_to_zero()
{
    // Span 1200, divisor 12. v=3000+11 -> (11)/12 = 0 (truncation)
    assert(compute_charge_percent(3011, 3000, 4200) == 0);
    // First mV that registers as 1% is +12
    assert(compute_charge_percent(3012, 3000, 4200) == 1);
}

static void test_percent_near_charged()
{
    // v=4199 (just below 4200): (1199)/12 = 99
    assert(compute_charge_percent(4199, 3000, 4200) == 99);
    // v=4188: 1188/12 = 99
    assert(compute_charge_percent(4188, 3000, 4200) == 99);
    // v=4187: 1187/12 = 98 (truncation)
    assert(compute_charge_percent(4187, 3000, 4200) == 98);
}

static void test_percent_clamp_kicks_in_with_tight_span()
{
    // span = 150 mV, divisor = 150/100 = 1 (truncated)
    // v_terminal = v_empty + 149 -> 149/1 = 149 -> clamped to 99
    assert(compute_charge_percent(3149, 3000, 3150) == 99);
}

// ----- compute_eta_time ----------------------------------------------------

static void test_eta_time_with_and_without_balance_port()
{
    EtaState s;
    s.etaDeltaSec = 10;
    s.percent_    = 0;

    // With balance port: factor 105 -> 10 * 105 = 1050
    assert(compute_eta_time(s, /*balance_port_connected=*/true)  == 1050);
    // Without: factor 100 -> 10 * 100 = 1000
    assert(compute_eta_time(s, false) == 1000);
}

static void test_eta_time_shrinks_with_progress()
{
    EtaState s;
    s.etaDeltaSec = 10;

    s.percent_ = 50;
    assert(compute_eta_time(s, true)  == 10 * (105 - 50));  // 550
    assert(compute_eta_time(s, false) == 10 * (100 - 50));  // 500

    s.percent_ = 99;
    assert(compute_eta_time(s, true)  == 10 * (105 - 99));  // 60
    assert(compute_eta_time(s, false) == 10 * (100 - 99));  // 10
}

// ----- update_eta_state (the bug) -----------------------------------------

static void test_eta_state_unchanged_when_no_percent_change()
{
    EtaState s;
    s.percent_         = 50;
    s.etaStartTimeCalc = 100;
    s.etaDeltaSec      = 7;

    // Same percent reported -> nothing updates
    update_eta_state(s, /*current_percent=*/50, /*time_sec=*/200);

    assert(s.percent_         == 50);
    assert(s.etaStartTimeCalc == 100);
    assert(s.etaDeltaSec      == 7);
}

static void test_eta_records_elapsed_time_on_first_percent_jump()
{
    // First percent jump at t=30. With the corrected subtraction
    // (getTimeSec - etaStartTimeCalc), etaSec = 30 - 0 = 30.
    // This is the time the 0%->1% step took.
    //
    // (Historic note: the firmware originally had the operands
    // reversed, producing UINT32_MAX-29 instead. Pre-fix tests
    // asserted that value here — see git history.)
    EtaState s;
    update_eta_state(s, /*current_percent=*/1, /*time_sec=*/30);

    assert(s.percent_         == 1);
    assert(s.etaStartTimeCalc == 30);
    assert(s.etaDeltaSec      == 30);

    // Plausible ETA: 30 s/percent * (105 - 1) = 3120 s
    uint32_t eta = compute_eta_time(s, /*balance_port_connected=*/true);
    assert(eta == 30 * 104);
}

static void test_eta_stays_pessimistic_when_subsequent_jumps_are_faster()
{
    // Second jump happened FASTER (45-30 = 15 s) than the first (30 s).
    // The "find longer time for deltapercent" branch is gated on
    // etaSec > etaDeltaSec, so etaDeltaSec stays at the slower value.
    // This is the pessimistic-by-design behaviour: ETA never shrinks
    // based on a single faster percent.
    EtaState s;
    update_eta_state(s, 1, 30);   // first jump, 30 s
    update_eta_state(s, 2, 45);   // second jump, 15 s

    assert(s.percent_         == 2);
    assert(s.etaStartTimeCalc == 45);
    assert(s.etaDeltaSec      == 30);  // kept the slower step
}

static void test_eta_grows_when_subsequent_jump_is_slower()
{
    // Second jump took LONGER (90-30 = 60 s) than the first (30 s).
    // etaDeltaSec is updated to the new slower value.
    EtaState s;
    update_eta_state(s, 1, 30);
    update_eta_state(s, 2, 90);

    assert(s.percent_         == 2);
    assert(s.etaStartTimeCalc == 90);
    assert(s.etaDeltaSec      == 60);  // grew to slower step
}

int main()
{
    test_percent_below_empty_returns_zero();
    test_percent_at_or_above_charged_returns_99();
    test_percent_midpoint();
    test_percent_just_above_empty_truncates_to_zero();
    test_percent_near_charged();
    test_percent_clamp_kicks_in_with_tight_span();

    test_eta_time_with_and_without_balance_port();
    test_eta_time_shrinks_with_progress();

    test_eta_state_unchanged_when_no_percent_change();
    test_eta_records_elapsed_time_on_first_percent_jump();
    test_eta_stays_pessimistic_when_subsequent_jumps_are_faster();
    test_eta_grows_when_subsequent_jump_is_slower();

    std::puts("test_monitor_eta: OK (12 cases)");
    return 0;
}
