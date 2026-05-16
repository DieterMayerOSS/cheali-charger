#include "balancer_logic.h"

#include <bit>
#include <climits>

namespace cheali_sim {

uint8_t count_connected_cells(uint16_t connected_mask)
{
    return static_cast<uint8_t>(std::popcount(connected_mask));
}

uint16_t calculate_per_cell(uint16_t v, uint8_t cell_count)
{
    if (cell_count == 0) return 0;
    return v / cell_count;
}

bool is_calibration_required(uint16_t connected_mask,
                             std::span<const uint16_t> cells,
                             uint16_t balancer_error)
{
    if (connected_mask == 0) return false;

    uint16_t Vmin = UINT16_MAX;
    uint16_t Vmax = 0;
    for (size_t i = 0; i < cells.size(); ++i) {
        if (connected_mask & (1u << i)) {
            const uint16_t vi = cells[i];
            if (Vmax < vi) Vmax = vi;
            if (vi < Vmin) Vmin = vi;
        }
    }
    return static_cast<uint16_t>(Vmax - Vmin) > balancer_error;
}

uint16_t calculate_balance(int8_t min_cell_index,
                           uint16_t connected_mask,
                           std::span<const uint16_t> cells)
{
    if (min_cell_index < 0) return 0;

    const uint16_t vmin = cells[static_cast<size_t>(min_cell_index)];
    uint16_t retu = 0;
    uint16_t cell_bit = 1;
    for (size_t c = 0; c < cells.size(); ++c) {
        if (connected_mask & cell_bit) {
            if (cells[c] > vmin) {
                retu |= cell_bit;
            }
        }
        cell_bit <<= 1;
    }
    return retu;
}

bool is_max_vout(uint16_t connected_mask,
                 std::span<const uint16_t> cells,
                 uint16_t max_v)
{
    for (size_t c = 0; c < cells.size(); ++c) {
        if (connected_mask & (1u << c)) {
            if (cells[c] >= max_v) return true;
        }
    }
    return false;
}

bool is_min_vout(uint16_t connected_mask,
                 std::span<const uint16_t> cells,
                 uint16_t min_v)
{
    for (size_t c = 0; c < cells.size(); ++c) {
        if (connected_mask & (1u << c)) {
            if (cells[c] <= min_v) return true;
        }
    }
    return false;
}

}  // namespace cheali_sim
