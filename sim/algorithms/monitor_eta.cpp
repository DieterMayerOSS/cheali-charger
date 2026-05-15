#include "monitor_eta.h"

namespace cheali_sim {

namespace {
constexpr uint8_t ETA_FACTOR_WITH_BALANCE_PORT    = 105;
constexpr uint8_t ETA_FACTOR_WITHOUT_BALANCE_PORT = 100;
}  // namespace

uint8_t compute_charge_percent(uint16_t v_terminal,
                               uint16_t v_empty,
                               uint16_t v_charged)
{
    uint16_t v1 = v_empty;
    uint16_t v2 = v_charged;
    uint16_t v  = v_terminal;

    if (v >= v2) return 99;
    if (v <= v1) return 0;

    v  -= v1;
    v2 -= v1;
    v2 /= 100;     // truncating; UB if v2 < 100 (preserved from firmware)
    v   = v / v2;

    if (v > 99) v = 99;  // "not 101% with isCharge"
    return static_cast<uint8_t>(v);
}

void update_eta_state(EtaState& state,
                      uint8_t current_percent,
                      uint32_t time_sec)
{
    if (state.procent_ < current_percent) {
        state.procent_ = current_percent;
        uint32_t etaSec = time_sec - state.etaStartTimeCalc;
        state.etaStartTimeCalc = time_sec;
        if (etaSec > state.etaDeltaSec) {
            state.etaDeltaSec = etaSec;  // "find longer time for deltaprocent"
        }
    }
}

uint32_t compute_eta_time(const EtaState& state, bool balance_port_connected)
{
    uint8_t kx = balance_port_connected ? ETA_FACTOR_WITH_BALANCE_PORT
                                        : ETA_FACTOR_WITHOUT_BALANCE_PORT;
    return state.etaDeltaSec * static_cast<uint32_t>(kx - state.procent_);
}

}  // namespace cheali_sim
