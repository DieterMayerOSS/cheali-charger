#pragma once

// Pure-logic decision cores extracted from the charging strategies:
//   - src/core/strategy/SimpleChargeStrategy.cpp
//   - src/core/strategy/DeltaChargeStrategy.cpp
//   - src/core/strategy/TheveninChargeStrategy.cpp
//
// Each firmware doStrategy() function mixes decision-making with
// hardware side-effects (SMPS::trySetIout, Balancer::isWorking, etc.).
// We port the *decisions* — i.e. "given these inputs, what status
// should we return / what flags should the caller set?" — leaving
// the hardware side outside.

#include <cstdint>

namespace cheali_sim {

// Mirrors Strategy::statusType from src/core/strategy/Strategy.h
enum class StrategyStatus : uint8_t {
    Error    = 0,
    Complete = 1,
    Running  = 2,
};

// ---- SimpleChargeStrategy -------------------------------------------------

// Core decision of SimpleChargeStrategy::doStrategy().
// If terminal voltage exceeds the configured end voltage, charge stops
// with ERROR (overvoltage protection). Otherwise charging continues.
//
// Strict > (matches firmware): equality returns Running.
StrategyStatus simple_charge_decide(uint16_t v_battery, uint16_t v_end);

// ---- DeltaChargeStrategy --------------------------------------------------

// Inputs for the delta-V / delta-T decision logic. Names mirror firmware
// values; see comments below for derivation in real firmware.
struct DeltaChargeInputs {
    uint16_t v_battery;             // AnalogInputs::getVbattery()
    uint16_t v_end;                 // Strategy::endV
    uint16_t v_discharged;          // ProgramData::getVoltage(VDischarged)
    uint16_t delta_count;           // AnalogInputs::getDeltaCount()
    int16_t  delta_textern;         // AnalogInputs::getRealValue(deltaTextern)
    int16_t  delta_vout;            // AnalogInputs::getRealValue(deltaVout)
    int16_t  delta_t_limit;         // ProgramData::getDeltaTLimit()
    int16_t  delta_v_limit;         // ProgramData::getDeltaVLimit()
    uint16_t delta_v_ignore_min;    // ProgramData::battery.deltaVIgnoreTime
    uint16_t delta_counts_per_min;  // DELTA_COUNTS_PER_MINUTE
    bool     enable_extern_t;       // ProgramData::battery.enable_externT
    bool     enable_delta_v;        // ProgramData::battery.enable_deltaV
};

// Outputs of the delta-charge decision. Beyond the status, two side-effect
// requests the firmware also issues during this call.
struct DeltaChargeOutputs {
    StrategyStatus status;
    bool set_iout_to_max;        // "kick up current" — firmware calls
                                 // SMPS::trySetIout(maxI) when Vbattery
                                 // climbs above v_discharged.
    bool enable_delta_vout_max;  // firmware calls enableDeltaVoutMax(...)
                                 // after the early-charge ignore window.
};

// Core decision of DeltaChargeStrategy::doStrategy().
// Encodes the early-termination logic for NiCd/NiMH-style charging:
//   - overvoltage -> Complete
//   - external temperature delta exceeded -> Complete
//   - negative-dV detected after ignore window -> Complete
DeltaChargeOutputs delta_charge_decide(const DeltaChargeInputs& in);

// ---- TheveninChargeStrategy ----------------------------------------------

// Core predicate of TheveninChargeStrategy::isEndVout().
// "End-of-charge voltage reached" means either:
//   - any individual cell hit its per-cell limit, OR
//   - (only when the balancer isn't currently running) the pack voltage
//     reached the overall end voltage.
//
// The "only when balancer not working" gate exists because while
// balancing, individual cell readings drift around — the per-pack
// reading is the only reliable comparison then.
bool thevenin_is_end_vout(uint16_t v_battery,
                          uint16_t v_end,
                          bool any_cell_at_per_cell_limit,
                          bool balancer_working);

}  // namespace cheali_sim
