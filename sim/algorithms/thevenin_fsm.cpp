#include "thevenin_fsm.h"

namespace cheali_sim {

TheveninTransitionResult thevenin_step_calc_new_i(TheveninState current,
                                                  bool is_end_vout)
{
    TheveninTransitionResult r{current, {}};

    switch (current) {
    case TheveninState::ConstantCurrentBalancing:
        if (!is_end_vout) break;
        r.effects.end_balancing = true;
        r.next_state            = TheveninState::ConstantCurrent;
        break;

    case TheveninState::ConstantCurrent:
        if (!is_end_vout) break;
        r.next_state           = TheveninState::LastRthMesurment;
        r.effects.zero_current = true;
        break;

    case TheveninState::LastRthMesurment:
        r.effects.zero_current = true;
        r.next_state           = TheveninState::LastConstantCurrent;
        break;

    case TheveninState::LastConstantCurrent:
        if (is_end_vout) {
            r.next_state = TheveninState::ConstantVoltageBalancing;
        }
        break;

    case TheveninState::ConstantVoltageBalancing:
    default:
        // Firmware writes `default: state_ = ConstantVoltageBalancing` —
        // a no-op when we're already there, but a "trap" that forces any
        // future un-handled state into the terminal phase.
        r.next_state = TheveninState::ConstantVoltageBalancing;
        break;
    }

    return r;
}

TheveninTransitionResult thevenin_step_balance_check(TheveninState current,
                                                     bool do_balance,
                                                     bool is_end_vout)
{
    TheveninTransitionResult r{current, {}};
    if (do_balance &&
        is_end_vout &&
        current == TheveninState::ConstantCurrentBalancing) {
        r.next_state            = TheveninState::ConstantCurrent;
        r.effects.end_balancing = true;
    }
    return r;
}

}  // namespace cheali_sim
