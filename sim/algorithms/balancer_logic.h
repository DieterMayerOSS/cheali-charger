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
// QUIRK preserved verbatim: when no cells are connected, the firmware
// initialises Vmin=UINT16_MAX, Vmax=0, never updates them, and then
// computes (Vmax - Vmin) in uint16_t — which underflows to 1.
// That value is then compared to balancer_error, which is always >=2
// in any sane configuration, so the function returns false. The quirky
// "1" never causes a real bug, but a refactor that "fixes" this without
// understanding it might introduce one.
bool is_calibration_required(uint16_t connected_mask,
                             const uint16_t* cell_voltages,
                             uint8_t max_cells,
                             uint16_t balancer_error);

}  // namespace cheali_sim
