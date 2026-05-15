#include "find_min_cell.h"

#include <cassert>
#include <cstdio>

int main()
{
    // No cells connected -> -1
    {
        uint16_t v[6] = {0};
        assert(find_min_cell(0, v, 6) == -1);
    }

    // Single connected cell is trivially the minimum
    {
        uint16_t v[6] = {3700, 3800, 3650, 0, 0, 0};
        assert(find_min_cell(0b000001, v, 6) == 0);
    }

    // Lowest of several connected cells
    {
        uint16_t v[6] = {3700, 3800, 3650, 3900, 0, 0};
        assert(find_min_cell(0b001111, v, 6) == 2);
    }

    // Disconnected cells are ignored, even if their value would be lower
    {
        uint16_t v[6] = {3700, 3800, 1000, 3900, 0, 0};
        assert(find_min_cell(0b001011, v, 6) == 0);
    }

    // Ties: first match wins (matches firmware behavior — `vmin > v` is strict)
    {
        uint16_t v[6] = {3700, 3700, 3700, 0, 0, 0};
        assert(find_min_cell(0b000111, v, 6) == 0);
    }

    std::puts("test_find_min_cell: OK");
    return 0;
}
