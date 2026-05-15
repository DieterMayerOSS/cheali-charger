#include "find_min_cell.h"

#include <climits>

int find_min_cell(uint16_t connected_mask,
                  const uint16_t* cell_voltages,
                  uint8_t max_cells)
{
    int min_cell = -1;
    uint16_t vmin = UINT16_MAX;
    for (uint8_t i = 0; i < max_cells; ++i) {
        if (connected_mask & (1u << i)) {
            uint16_t v = cell_voltages[i];
            if (vmin > v) {
                min_cell = i;
                vmin = v;
            }
        }
    }
    return min_cell;
}
