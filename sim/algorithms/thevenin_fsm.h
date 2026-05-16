#pragma once

// Explicit FSM port for TheveninMethod from src/core/strategy/TheveninMethod.cpp.
//
// The firmware tangles the state-machine transitions inside two larger
// functions, calculateNewI() and balance_isComplete(), each of which
// also has side effects on currents and balancer state. We extract the
// pure state-transition logic into two transition functions, so the
// FSM topology becomes explicit and testable.
//
// FSM topology (derived from reading the firmware):
//
//   [Initial]
//      ↓
//   ConstantCurrentBalancing
//      │ isEndVout   (also fires in balance_isComplete — DUPLICATE)
//      │ end Balancer
//      ↓
//   ConstantCurrent
//      │ isEndVout
//      │ newI := 0
//      ↓
//   LastRthMeasurement
//      │ unconditional
//      │ newI := 0
//      ↓
//   LastConstantCurrent
//      │ isEndVout
//      ↓
//   ConstantVoltageBalancing  (terminal, self-loops via default:)
//
// HISTORY: an unreachable RthMeasurement state used to live in the
// enum, with a switch case that routed it back to ConstantCurrentBalancing.
// Nothing in the firmware ever assigned state_ = RthMeasurement, so the
// case was pure dead code. Removed in this fork; see git history.
//
// DUPLICATE TRANSITION: ConstantCurrentBalancing → ConstantCurrent
// fires in BOTH balance_isComplete() AND calculateNewI(). In normal
// operation, balance_isComplete runs first; by the time calculateNewI's
// switch sees the state, it's usually already ConstantCurrent and the
// switch case is a no-op. But the firmware doesn't make this ordering
// explicit, and a refactor that changes the call order could expose
// the redundancy as a bug.

#include <cstdint>

namespace cheali_sim {

enum class TheveninState : uint8_t {
    ConstantCurrentBalancing,   // initial state, CC charge with balancing active
    ConstantCurrent,            // CC charge after balancing ended
    LastRthMeasurement,           // final Rth probe before CV
    LastConstantCurrent,        // final CC step
    ConstantVoltageBalancing,   // terminal CV phase
};

// Side effects requested by a transition.
struct TheveninTransitionEffects {
    bool end_balancing = false;  // call Balancer::endBalancing()
    bool zero_current  = false;  // set newI_ = 0
};

struct TheveninTransitionResult {
    TheveninState              next_state;
    TheveninTransitionEffects  effects;
};

// Port of the switch statement in TheveninMethod::calculateNewI().
// Returns the new state and any side effects that the firmware would
// have applied alongside the transition.
//
// Note: this is the *transition* logic only. The firmware gates the
// switch behind a stability check (`isOutStable && !Balancer::isWorking`);
// callers must apply that gate themselves before calling this.
TheveninTransitionResult thevenin_step_calc_new_i(TheveninState current,
                                                  bool is_end_vout);

// Port of the first if-block in TheveninMethod::balance_isComplete():
//
//   if (doBalance && isEndVout && state_ == CC_Balancing) {
//       state_ = ConstantCurrent;
//       end Balancer
//   }
//
// Returns the (possibly unchanged) next state plus side-effect flags.
TheveninTransitionResult thevenin_step_balance_check(TheveninState current,
                                                     bool do_balance,
                                                     bool is_end_vout);

}  // namespace cheali_sim
