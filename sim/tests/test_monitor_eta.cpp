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
    s.procent_    = 0;

    // With balance port: factor 105 -> 10 * 105 = 1050
    assert(compute_eta_time(s, /*balance_port_connected=*/true)  == 1050);
    // Without: factor 100 -> 10 * 100 = 1000
    assert(compute_eta_time(s, false) == 1000);
}

static void test_eta_time_shrinks_with_progress()
{
    EtaState s;
    s.etaDeltaSec = 10;

    s.procent_ = 50;
    assert(compute_eta_time(s, true)  == 10 * (105 - 50));  // 550
    assert(compute_eta_time(s, false) == 10 * (100 - 50));  // 500

    s.procent_ = 99;
    assert(compute_eta_time(s, true)  == 10 * (105 - 99));  // 60
    assert(compute_eta_time(s, false) == 10 * (100 - 99));  // 10
}

// ----- update_eta_state (the bug) -----------------------------------------

static void test_eta_state_unchanged_when_no_percent_change()
{
    EtaState s;
    s.procent_         = 50;
    s.etaStartTimeCalc = 100;
    s.etaDeltaSec      = 7;

    // Same percent reported -> nothing updates
    update_eta_state(s, /*current_percent=*/50, /*time_sec=*/200);

    assert(s.procent_         == 50);
    assert(s.etaStartTimeCalc == 100);
    assert(s.etaDeltaSec      == 7);
}

static void test_monitor_eta_bug_subtraction_order()
{
    // FIRMWARE BUG (preserved): line 66 of Monitor.cpp computes
    //     etaSec = etaStartTimeCalc - getTimeSec()
    // which is reversed. With etaStartTimeCalc < getTimeSec(),
    // uint32_t underflow produces ~UINT32_MAX.
    //
    // Scenario: charge starts at t=0 with etaStartTimeCalc=0, percent=0.
    // First percent jump happens at t=30 (got from 0% to 1%).
    // Firmware computes etaSec = 0 - 30 = UINT32_MAX - 29.
    // Then etaDeltaSec is set to that huge value.
    EtaState s;
    update_eta_state(s, /*current_percent=*/1, /*time_sec=*/30);

    // procent_ advanced
    assert(s.procent_ == 1);
    // etaStartTimeCalc updated to current time AFTER the bogus subtraction
    assert(s.etaStartTimeCalc == 30);
    // etaDeltaSec corrupted by the underflow
    assert(s.etaDeltaSec == UINT32_MAX - 29);

    // The displayed ETA is now garbage:
    // (UINT32_MAX - 29) * (105 - 1) silently overflows uint32_t
    uint32_t eta = compute_eta_time(s, /*balance_port_connected=*/true);
    // We don't pin the wrapped value (it's UB-ish); we just assert it
    // is *not* a plausible "time remaining" — namely, not equal to
    // the intuitive "(60 - 0) * (105 - 1) / 60 = 104 minutes" or
    // anything close.
    // If the bug were fixed (subtraction in correct order), etaDeltaSec
    // would be 30 and eta would be 30 * 104 = 3120. We assert it's NOT
    // that, to make sure the bug is genuinely present.
    assert(eta != 30 * 104);
}

static void test_monitor_eta_bug_persists_on_subsequent_percents()
{
    // Second percent jump after the first. etaStartTimeCalc now holds
    // 30 (set during the previous call). At t=60, percent 1->2:
    //   etaSec = 30 - 60 = UINT32_MAX - 29 (same underflow, same value)
    //   etaSec > etaDeltaSec? UINT32_MAX-29 > UINT32_MAX-29 is FALSE,
    //   so etaDeltaSec stays at UINT32_MAX-29.
    EtaState s;
    update_eta_state(s, 1, 30);                         // first jump
    uint32_t delta_after_first = s.etaDeltaSec;

    update_eta_state(s, 2, 60);                         // second jump

    assert(s.procent_         == 2);
    assert(s.etaStartTimeCalc == 60);
    assert(s.etaDeltaSec      == delta_after_first);    // unchanged
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
    test_monitor_eta_bug_subtraction_order();
    test_monitor_eta_bug_persists_on_subsequent_percents();

    std::puts("test_monitor_eta: OK (11 cases)");
    return 0;
}
