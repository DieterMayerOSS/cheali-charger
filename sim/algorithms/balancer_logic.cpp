#include "balancer_logic.h"

#include <climits>

namespace cheali_sim {

uint8_t count_connected_cells(uint16_t connected_mask)
{
    uint8_t bits = 0;
    for (int8_t i = 0; i < 16; ++i) {
        if (connected_mask & 1u) ++bits;
        connected_mask >>= 1;
    }
    return bits;
}

uint16_t calculate_per_cell(uint16_t v, uint8_t cell_count)
{
    if (cell_count == 0) return 0;
    return v / cell_count;
}

bool is_calibration_required(uint16_t connected_mask,
                             const uint16_t* cell_voltages,
                             uint8_t max_cells,
                             uint16_t balancer_error)
{
    uint16_t Vmin = UINT16_MAX;
    uint16_t Vmax = 0;
    for (uint8_t i = 0; i < max_cells; ++i) {
        if (connected_mask & (1u << i)) {
            uint16_t vi = cell_voltages[i];
            if (Vmax < vi) Vmax = vi;
            if (vi < Vmin) Vmin = vi;
        }
    }
    // Mirrors firmware: uint16_t subtraction. When no cells were ever
    // considered, this underflows to 1 — see header comment.
    return static_cast<uint16_t>(Vmax - Vmin) > balancer_error;
}

uint16_t calculate_balance(int8_t min_cell_index,
                           uint16_t connected_mask,
                           const uint16_t* cell_voltages,
                           uint8_t max_cells)
{
    if (min_cell_index < 0) return 0;

    uint16_t vmin = cell_voltages[min_cell_index];
    uint16_t retu = 0;
    uint16_t cell_bit = 1;
    for (uint8_t c = 0; c < max_cells; ++c) {
        if (connected_mask & cell_bit) {
            uint16_t v = cell_voltages[c];
            if (v > vmin) {
                retu |= cell_bit;
            }
        }
        cell_bit <<= 1;
    }
    return retu;
}

bool is_max_vout(uint16_t connected_mask,
                 const uint16_t* cell_voltages,
                 uint8_t max_cells,
                 uint16_t max_v)
{
    for (uint8_t c = 0; c < max_cells; ++c) {
        if (connected_mask & (1u << c)) {
            if (cell_voltages[c] >= max_v) return true;
        }
    }
    return false;
}

bool is_min_vout(uint16_t connected_mask,
                 const uint16_t* cell_voltages,
                 uint8_t max_cells,
                 uint16_t min_v)
{
    for (uint8_t c = 0; c < max_cells; ++c) {
        if (connected_mask & (1u << c)) {
            if (cell_voltages[c] <= min_v) return true;
        }
    }
    return false;
}

}  // namespace cheali_sim
