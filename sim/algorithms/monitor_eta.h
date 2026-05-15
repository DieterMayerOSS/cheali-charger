#pragma once

// Pure C++ port of Monitor's ETA logic
// (src/core/strategy/Monitor.cpp: getChargeProcent, calculateDeltaProcentTimeSec,
//  getETATime). Verbatim semantics — including a real bug, see below.
//
// The factors 105 / 100 in compute_eta_time() are undocumented in the
// firmware. Best-guess interpretation:
//   - 100  = "100% remaining - current %" → linear extrapolation
//   - 105  = "100% + 5% padding for the post-CC balancing phase"
//            (only added if a balance port is connected, which is when
//             balancing will actually happen at the end of charge)

#include <cstdint>

namespace cheali_sim {

// Port of Monitor::getChargeProcent().
//
// Maps a measured pack voltage onto a [0..99]% scale linearly between
// v_empty and v_charged. Notes:
//   - Returns 99 (never 100) when v_terminal >= v_charged. 100 is
//     reserved for "complete" elsewhere in the firmware.
//   - Integer math: the divisor (v_charged - v_empty) / 100 is truncated.
//     With typical LiPo settings (e.g. 3.0V..4.2V → 1200 mV span, div=12)
//     this is fine. With (v_charged - v_empty) < 100 you'll hit divide-by-
//     zero — the firmware doesn't guard against this, and neither do we.
//     Callers must keep the span >= 100 mV.
//   - The "v > 99 → 99" clamp matters when the truncated divisor allows
//     intermediate results to exceed 99 (e.g. very tight voltage spans).
uint8_t compute_charge_percent(uint16_t v_terminal,
                               uint16_t v_empty,
                               uint16_t v_charged);

// Mirrors the persistent ETA state Monitor.cpp keeps in globals.
struct EtaState {
    uint32_t etaStartTimeCalc = 0;  // time of the last percent jump
    uint32_t etaDeltaSec      = 0;  // largest observed inter-percent interval
    uint8_t  procent_         = 0;  // last seen percent (sic: Polish-ism)
};

// Port of Monitor::calculateDeltaProcentTimeSec().
//
// Called every cycle. If the charge percent has just incremented since
// the last call, records how long that took and keeps the maximum
// observed interval (pessimistic ETA — the model assumes future
// percents won't go faster than the slowest one seen).
//
// HISTORY: the firmware originally had a reversed subtraction
// (etaStartTimeCalc - getTimeSec()) that underflowed uint32_t and
// blew etaDeltaSec up to ~UINT32_MAX, making the displayed ETA
// garbage from ~2013 until the fix in this fork.
void update_eta_state(EtaState& state,
                      uint8_t current_percent,
                      uint32_t time_sec);

// Port of Monitor::getETATime() — minus the embedded call to
// calculateDeltaProcentTimeSec (we keep that explicit so tests can
// drive the two stages separately).
//
// Returns etaDeltaSec * (kx - procent_), where kx = 105 if a balance
// port is connected, else 100. Result is uint32_t and can wrap silently
// if etaDeltaSec has been corrupted by the bug above.
uint32_t compute_eta_time(const EtaState& state, bool balance_port_connected);

}  // namespace cheali_sim
