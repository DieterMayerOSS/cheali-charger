#pragma once

#include <cstdint>

// Reference implementation of Balancer::getCellMinV()
// (see src/core/strategy/Balancer.cpp).
//
// Returns the index of the connected cell with the lowest voltage,
// or -1 if no cells are connected.
//
// Parameters:
//   connected_mask  - bitmask, bit i set => cell i is connected
//   cell_voltages   - array of length >= max_cells, in raw ADC units
//                     (UINT16_MAX is treated as "no reading")
//   max_cells       - number of cells to consider (MAX_BALANCE_CELLS)
int find_min_cell(uint16_t connected_mask,
                  const uint16_t* cell_voltages,
                  uint8_t max_cells);
