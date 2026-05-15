#include "charge_strategy_decisions.h"

namespace cheali_sim {

StrategyStatus simple_charge_decide(uint16_t v_battery, uint16_t v_end)
{
    if (v_battery > v_end) {
        return StrategyStatus::Error;
    }
    return StrategyStatus::Running;
}

DeltaChargeOutputs delta_charge_decide(const DeltaChargeInputs& in)
{
    DeltaChargeOutputs out{StrategyStatus::Running, false, false};

    // 1) "Kick up" current: if we're above the discharged floor,
    //    push current to max. (Firmware: SMPS::trySetIout(maxI).)
    if (in.v_discharged < in.v_battery) {
        out.set_iout_to_max = true;
    }

    // 2) Overvoltage stop. Note this returns COMPLETE, not ERROR — for
    //    NiMH/NiCd delta-V charging, overvoltage is a normal terminus.
    if (in.v_battery > in.v_end) {
        out.status = StrategyStatus::Complete;
        return out;
    }

    // 3) Need at least two delta samples before we can talk about a delta
    if (in.delta_count < 2) {
        return out;  // status stays Running
    }

    // 4) External-temperature delta cutoff
    if (in.enable_extern_t) {
        if (in.delta_textern > in.delta_t_limit) {
            out.status = StrategyStatus::Complete;
            return out;
        }
    }

    // 5) Ignore the first N minutes of -dV (terminal voltage isn't stable
    //    yet) but record that we entered the active window.
    const bool past_ignore_window =
        in.delta_count >=
        static_cast<uint16_t>(in.delta_v_ignore_min * in.delta_counts_per_min);
    out.enable_delta_vout_max = past_ignore_window;

    if (past_ignore_window && in.enable_delta_v) {
        // delta_v_limit is negative (a -dV trigger). If measured delta
        // goes BELOW that threshold, charging is done.
        if (in.delta_vout < in.delta_v_limit) {
            out.status = StrategyStatus::Complete;
            return out;
        }
    }

    return out;
}

bool thevenin_is_end_vout(uint16_t v_battery,
                          uint16_t v_end,
                          bool any_cell_at_per_cell_limit,
                          bool balancer_working)
{
    bool end = any_cell_at_per_cell_limit;
    if (!balancer_working) {
        end = end || (v_end <= v_battery);
    }
    return end;
}

}  // namespace cheali_sim
