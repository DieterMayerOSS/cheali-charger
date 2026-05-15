#pragma once

// Pure C++ ports of pieces of Balancer (src/core/strategy/Balancer.cpp).
// Verbatim semantics — tests pin down current behaviour, including
// quirks that may or may not be intentional.

#include <cstdint>

namespace cheali_sim {

// Port of countBits() from src/core/Utils.cpp.
// Counts set bits in a 16-bit value (e.g. connectedBalancePortCells).
uint8_t count_connected_cells(uint16_t connected_mask);

// Port of Balancer::calculatePerCell().
// Divides v evenly among the connected cells. Returns 0 if cell_count is 0
// (firmware guard against div-by-zero).
//
// Integer division — truncating, no rounding.
uint16_t calculate_per_cell(uint16_t v, uint8_t cell_count);

// Port of Balancer::isCalibrationRequired().
// Returns true iff (Vmax - Vmin) among *connected* cells exceeds
// balancer_error (strict >).
//
// HISTORY: previously had a uint16_t underflow path when no cells
// were connected (Vmax=0, Vmin=UINT16_MAX -> Vmax-Vmin=1). Fixed
// in this fork by an early return when connected_mask == 0.
bool is_calibration_required(uint16_t connected_mask,
                             const uint16_t* cell_voltages,
                             uint8_t max_cells,
                             uint16_t balancer_error);

// Port of Balancer::calculateBalance().
// Returns a bitmask of cells that need discharging during balancing —
// every connected cell whose voltage is strictly greater than the
// voltage at min_cell_index.
//
// Returns 0 if min_cell_index < 0 (firmware sentinel meaning "no min
// cell has been picked yet"). Caller must ensure min_cell_index is
// either negative or within [0, max_cells); the firmware doesn't
// bounds-check the lookup either.
uint16_t calculate_balance(int8_t min_cell_index,
                           uint16_t connected_mask,
                           const uint16_t* cell_voltages,
                           uint8_t max_cells);

// Port of Balancer::isMaxVout().
// Returns true iff any connected cell's voltage is >= max_v.
// Used to detect "any cell reached the upper voltage limit".
bool is_max_vout(uint16_t connected_mask,
                 const uint16_t* cell_voltages,
                 uint8_t max_cells,
                 uint16_t max_v);

// Port of Balancer::isMinVout().
// Returns true iff any connected cell's voltage is <= min_v.
// Used to detect "any cell hit the discharge cutoff".
bool is_min_vout(uint16_t connected_mask,
                 const uint16_t* cell_voltages,
                 uint8_t max_cells,
                 uint16_t min_v);

}  // namespace cheali_sim
