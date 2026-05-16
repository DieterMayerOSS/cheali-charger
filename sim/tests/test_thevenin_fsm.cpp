#include "thevenin_fsm.h"

#include <cassert>
#include <cstdio>
#include <initializer_list>

using cheali_sim::TheveninState;
using cheali_sim::TheveninTransitionResult;
using cheali_sim::thevenin_step_calc_new_i;
using cheali_sim::thevenin_step_balance_check;

// ---- thevenin_step_calc_new_i: per-state transitions ---------------------

static void test_cc_balancing_stays_when_not_end_vout()
{
    auto r = thevenin_step_calc_new_i(TheveninState::ConstantCurrentBalancing, false);
    assert(r.next_state == TheveninState::ConstantCurrentBalancing);
    assert(r.effects.end_balancing == false);
    assert(r.effects.zero_current  == false);
}

static void test_cc_balancing_transitions_to_cc_on_end_vout()
{
    auto r = thevenin_step_calc_new_i(TheveninState::ConstantCurrentBalancing, true);
    assert(r.next_state == TheveninState::ConstantCurrent);
    assert(r.effects.end_balancing == true);
    assert(r.effects.zero_current  == false);
}

static void test_cc_stays_when_not_end_vout()
{
    auto r = thevenin_step_calc_new_i(TheveninState::ConstantCurrent, false);
    assert(r.next_state == TheveninState::ConstantCurrent);
    assert(r.effects.zero_current == false);
}

static void test_cc_transitions_to_last_rth_with_zero_current()
{
    auto r = thevenin_step_calc_new_i(TheveninState::ConstantCurrent, true);
    assert(r.next_state == TheveninState::LastRthMeasurement);
    assert(r.effects.zero_current == true);
    assert(r.effects.end_balancing == false);
}

static void test_last_rth_unconditional_with_zero_current()
{
    auto r_true  = thevenin_step_calc_new_i(TheveninState::LastRthMeasurement, true);
    auto r_false = thevenin_step_calc_new_i(TheveninState::LastRthMeasurement, false);
    // No conditional on is_end_vout — always advances
    assert(r_true.next_state  == TheveninState::LastConstantCurrent);
    assert(r_false.next_state == TheveninState::LastConstantCurrent);
    assert(r_true.effects.zero_current  == true);
    assert(r_false.effects.zero_current == true);
}

static void test_last_cc_stays_when_not_end_vout()
{
    auto r = thevenin_step_calc_new_i(TheveninState::LastConstantCurrent, false);
    assert(r.next_state == TheveninState::LastConstantCurrent);
}

static void test_last_cc_transitions_to_cv_on_end_vout()
{
    auto r = thevenin_step_calc_new_i(TheveninState::LastConstantCurrent, true);
    assert(r.next_state == TheveninState::ConstantVoltageBalancing);
}

static void test_cv_balancing_self_loops()
{
    // Terminal state — caught by `default:`, self-loops both ways.
    auto r_true  = thevenin_step_calc_new_i(TheveninState::ConstantVoltageBalancing, true);
    auto r_false = thevenin_step_calc_new_i(TheveninState::ConstantVoltageBalancing, false);
    assert(r_true.next_state  == TheveninState::ConstantVoltageBalancing);
    assert(r_false.next_state == TheveninState::ConstantVoltageBalancing);
}

// ---- thevenin_step_balance_check ------------------------------------------

static void test_balance_check_does_nothing_when_not_balancing()
{
    auto r = thevenin_step_balance_check(TheveninState::ConstantCurrentBalancing,
                                         /*do_balance=*/false,
                                         /*is_end_vout=*/true);
    assert(r.next_state == TheveninState::ConstantCurrentBalancing);
    assert(r.effects.end_balancing == false);
}

static void test_balance_check_does_nothing_without_end_vout()
{
    auto r = thevenin_step_balance_check(TheveninState::ConstantCurrentBalancing,
                                         true, false);
    assert(r.next_state == TheveninState::ConstantCurrentBalancing);
    assert(r.effects.end_balancing == false);
}

static void test_balance_check_does_nothing_in_wrong_state()
{
    // Only fires from ConstantCurrentBalancing — other states are ignored
    for (auto s : {TheveninState::ConstantCurrent,
                   TheveninState::LastRthMeasurement,
                   TheveninState::LastConstantCurrent,
                   TheveninState::ConstantVoltageBalancing}) {
        auto r = thevenin_step_balance_check(s, true, true);
        assert(r.next_state == s);
        assert(r.effects.end_balancing == false);
    }
}

static void test_balance_check_fires_when_all_conditions_met()
{
    auto r = thevenin_step_balance_check(TheveninState::ConstantCurrentBalancing,
                                         /*do_balance=*/true,
                                         /*is_end_vout=*/true);
    assert(r.next_state == TheveninState::ConstantCurrent);
    assert(r.effects.end_balancing == true);
}

// ---- the DUPLICATE transition: balance_check + calc_new_i in sequence -----

static void test_duplicate_cc_balancing_to_cc_transition_in_sequence()
{
    // In firmware, balance_isComplete runs first, then calculateNewI.
    // BOTH have a CC_Balancing -> CC transition. Verify the redundancy:
    // after balance_check has fired the transition, calc_new_i sees CC
    // and does nothing (because is_end_vout matches CC's "go to LastRth"
    // path — which is a SEPARATE side effect, not a redundant
    // CC_Balancing -> CC redirect).

    // Step 1: balance_check fires the CC_Balancing -> CC transition
    auto step1 = thevenin_step_balance_check(TheveninState::ConstantCurrentBalancing,
                                             true, true);
    assert(step1.next_state == TheveninState::ConstantCurrent);
    assert(step1.effects.end_balancing == true);

    // Step 2: calc_new_i runs on the new state. Now we're in CC with
    // is_end_vout still true → advances to LastRth.
    auto step2 = thevenin_step_calc_new_i(step1.next_state, true);
    assert(step2.next_state == TheveninState::LastRthMeasurement);
    assert(step2.effects.zero_current == true);

    // Net effect of one firmware cycle: CC_Balancing -> CC -> LastRth.
    // I.e. with is_end_vout sustained, the FSM skips a step compared to
    // what a naive read of calc_new_i alone would suggest.
}

// ---- happy-path full sequence ---------------------------------------------

static void test_full_charge_sequence()
{
    // Simulate a complete walk through the FSM with is_end_vout flipping
    // at the right moments. We model "is_end_vout" being false during
    // CC charging and true once the voltage reaches the limit. This
    // ignores the firmware's gating (isOutStable, !isWorking) — those
    // are timing gates, not state transitions.

    TheveninState s = TheveninState::ConstantCurrentBalancing;

    // CC charging, not yet at end voltage -> stay
    s = thevenin_step_calc_new_i(s, false).next_state;
    assert(s == TheveninState::ConstantCurrentBalancing);

    // End voltage reached: balance_check fires first (state advances)
    s = thevenin_step_balance_check(s, true, true).next_state;
    assert(s == TheveninState::ConstantCurrent);

    // calc_new_i runs after: CC + is_end_vout -> LastRth (zero current)
    auto r1 = thevenin_step_calc_new_i(s, true);
    s = r1.next_state;
    assert(s == TheveninState::LastRthMeasurement);
    assert(r1.effects.zero_current);

    // LastRth: unconditional advance, zero current
    auto r2 = thevenin_step_calc_new_i(s, false);
    s = r2.next_state;
    assert(s == TheveninState::LastConstantCurrent);
    assert(r2.effects.zero_current);

    // LastCC: stays if not at end voltage
    s = thevenin_step_calc_new_i(s, false).next_state;
    assert(s == TheveninState::LastConstantCurrent);

    // LastCC: advances on end voltage
    s = thevenin_step_calc_new_i(s, true).next_state;
    assert(s == TheveninState::ConstantVoltageBalancing);

    // CV: terminal, stays
    s = thevenin_step_calc_new_i(s, true).next_state;
    assert(s == TheveninState::ConstantVoltageBalancing);
    s = thevenin_step_calc_new_i(s, false).next_state;
    assert(s == TheveninState::ConstantVoltageBalancing);
}

int main()
{
    // calc_new_i per-state
    test_cc_balancing_stays_when_not_end_vout();
    test_cc_balancing_transitions_to_cc_on_end_vout();
    test_cc_stays_when_not_end_vout();
    test_cc_transitions_to_last_rth_with_zero_current();
    test_last_rth_unconditional_with_zero_current();
    test_last_cc_stays_when_not_end_vout();
    test_last_cc_transitions_to_cv_on_end_vout();
    test_cv_balancing_self_loops();

    // balance_check per-state
    test_balance_check_does_nothing_when_not_balancing();
    test_balance_check_does_nothing_without_end_vout();
    test_balance_check_does_nothing_in_wrong_state();
    test_balance_check_fires_when_all_conditions_met();

    // Interaction & full sequence
    test_duplicate_cc_balancing_to_cc_transition_in_sequence();
    test_full_charge_sequence();

    std::puts("test_thevenin_fsm: OK (14 cases)");
    return 0;
}
