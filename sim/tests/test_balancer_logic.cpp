#include "balancer_logic.h"

#include <cassert>
#include <cstdio>

using cheali_sim::count_connected_cells;
using cheali_sim::calculate_per_cell;
using cheali_sim::is_calibration_required;
using cheali_sim::calculate_balance;
using cheali_sim::is_max_vout;
using cheali_sim::is_min_vout;

static void test_count_connected_cells()
{
    assert(count_connected_cells(0)        == 0);
    assert(count_connected_cells(0b1)      == 1);
    assert(count_connected_cells(0b101010) == 3);
    assert(count_connected_cells(0xFFFF)   == 16);
    // Higher bits beyond MAX_BALANCE_CELLS still counted (firmware
    // counts all 16 bits, not just the "valid" ones)
    assert(count_connected_cells(0x8000)   == 1);
}

static void test_calculate_per_cell()
{
    // Zero cell guard
    assert(calculate_per_cell(4200, 0) == 0);

    // Zero input
    assert(calculate_per_cell(0, 6) == 0);

    // Normal: 4200mV pack / 1 cell = 4200mV per cell
    assert(calculate_per_cell(4200, 1) == 4200);

    // 12600mV (3S LiPo, fully charged) / 3 = 4200mV
    assert(calculate_per_cell(12600, 3) == 4200);

    // Integer division truncates, no rounding
    assert(calculate_per_cell(7, 3) == 2);   // 7/3 == 2 (not 2.33)
    assert(calculate_per_cell(10, 3) == 3);  // 10/3 == 3 (not 3.33)

    // 6S LiPo
    assert(calculate_per_cell(25200, 6) == 4200);
}

static void test_is_calibration_required_basics()
{
    uint16_t v[6] = {3700, 3700, 3700, 3700, 3700, 3700};

    // All cells equal -> diff = 0 -> not required (regardless of threshold)
    assert(is_calibration_required(0b111111, v, 6, 5) == false);

    // Threshold = 0 with all cells equal still false (strict >)
    assert(is_calibration_required(0b111111, v, 6, 0) == false);
}

static void test_is_calibration_required_threshold()
{
    // Cell 0 = 3700, Cell 1 = 3710 -> diff = 10
    uint16_t v[6] = {3700, 3710, 3700, 3700, 3700, 3700};

    // diff=10, threshold=9 -> required (10 > 9)
    assert(is_calibration_required(0b111111, v, 6, 9) == true);

    // diff=10, threshold=10 -> NOT required (strict > only)
    assert(is_calibration_required(0b111111, v, 6, 10) == false);

    // diff=10, threshold=11 -> not required
    assert(is_calibration_required(0b111111, v, 6, 11) == false);
}

static void test_is_calibration_required_ignores_disconnected()
{
    // Cell 2 is way off — but it's not connected. Should be ignored.
    uint16_t v[6] = {3700, 3710, 1000, 3700, 3700, 3700};

    // Without cell 2: diff = 10
    assert(is_calibration_required(0b111011, v, 6, 100) == false);

    // With cell 2 in the mask: diff = 2710 -> definitely required
    assert(is_calibration_required(0b111111, v, 6, 100) == true);
}

static void test_is_calibration_required_no_cells_connected_quirk()
{
    // QUIRK: with mask=0, the inner loop body never runs, so
    // Vmin=UINT16_MAX, Vmax=0. Then (Vmax - Vmin) in uint16 = 1.
    //
    // For any realistic balancer_error >= 2, the function returns false.
    // This test pins down that quirky behaviour so any refactor must
    // either preserve it or consciously change it.
    uint16_t v[6] = {0};

    // balancer_error = 0 -> 1 > 0 -> TRUE (quirky outcome!)
    assert(is_calibration_required(0, v, 6, 0) == true);

    // balancer_error = 1 -> 1 > 1 -> false
    assert(is_calibration_required(0, v, 6, 1) == false);

    // balancer_error = 2 (realistic minimum) -> 1 > 2 -> false
    assert(is_calibration_required(0, v, 6, 2) == false);

    // Any realistic balancer_error -> false
    assert(is_calibration_required(0, v, 6, 50) == false);
}

static void test_is_calibration_required_single_cell()
{
    // Only one cell connected -> Vmin == Vmax -> diff = 0 -> never required
    uint16_t v[6] = {3700, 9999, 9999, 9999, 9999, 9999};
    assert(is_calibration_required(0b000001, v, 6, 0) == false);
    assert(is_calibration_required(0b000001, v, 6, 50) == false);
}

static void test_is_calibration_required_extremes()
{
    // Min on first cell, max on last cell of a 6S pack
    uint16_t v[6] = {3000, 4100, 4100, 4100, 4100, 4200};
    // diff = 4200 - 3000 = 1200
    assert(is_calibration_required(0b111111, v, 6, 1199) == true);
    assert(is_calibration_required(0b111111, v, 6, 1200) == false);
}

static void test_calculate_balance_no_min_cell()
{
    // min_cell_index == -1 -> firmware sentinel "no min picked" -> returns 0
    uint16_t v[6] = {3700, 3800, 3900, 4000, 4100, 4200};
    assert(calculate_balance(-1, 0b111111, v, 6) == 0);
}

static void test_calculate_balance_all_equal()
{
    // All cells equal -> no cell is *strictly greater* than vmin -> 0
    uint16_t v[6] = {3700, 3700, 3700, 3700, 3700, 3700};
    assert(calculate_balance(0, 0b111111, v, 6) == 0);
}

static void test_calculate_balance_picks_higher_cells()
{
    // min_cell = 0 (3700). Cells 1,2,3,4,5 all higher -> bits 1..5 set
    uint16_t v[6] = {3700, 3750, 3800, 3850, 3900, 3950};
    uint16_t mask = calculate_balance(0, 0b111111, v, 6);
    assert(mask == 0b111110);
}

static void test_calculate_balance_skips_disconnected()
{
    // Cell 2 is high but disconnected -> should not be included even
    // though its voltage would qualify.
    uint16_t v[6] = {3700, 3750, 9999, 3850, 3900, 3950};
    uint16_t mask = calculate_balance(0, 0b111011, v, 6);
    assert(mask == 0b111010);  // bit 2 must NOT be set
}

static void test_calculate_balance_strict_gt()
{
    // Cell at exactly vmin (== vmin) is NOT discharged — strict > only
    uint16_t v[6] = {3700, 3700, 3800, 0, 0, 0};
    uint16_t mask = calculate_balance(0, 0b000111, v, 6);
    assert(mask == 0b000100);  // only cell 2, not cell 1
}

static void test_is_max_vout()
{
    // None at/above limit
    {
        uint16_t v[6] = {3700, 3800, 3900, 4000, 4100, 4150};
        assert(is_max_vout(0b111111, v, 6, 4200) == false);
    }
    // Exactly at limit -> true (uses >=)
    {
        uint16_t v[6] = {3700, 3800, 3900, 4000, 4100, 4200};
        assert(is_max_vout(0b111111, v, 6, 4200) == true);
    }
    // Above limit -> true
    {
        uint16_t v[6] = {3700, 3800, 3900, 4000, 4100, 4250};
        assert(is_max_vout(0b111111, v, 6, 4200) == true);
    }
    // Disconnected high cell ignored
    {
        uint16_t v[6] = {3700, 3800, 9999, 3900, 4000, 4100};
        assert(is_max_vout(0b111011, v, 6, 4200) == false);
    }
}

static void test_is_min_vout()
{
    // None at/below limit
    {
        uint16_t v[6] = {3100, 3200, 3300, 3400, 3500, 3600};
        assert(is_min_vout(0b111111, v, 6, 3000) == false);
    }
    // Exactly at limit -> true (uses <=)
    {
        uint16_t v[6] = {3000, 3200, 3300, 3400, 3500, 3600};
        assert(is_min_vout(0b111111, v, 6, 3000) == true);
    }
    // Below limit -> true
    {
        uint16_t v[6] = {2900, 3200, 3300, 3400, 3500, 3600};
        assert(is_min_vout(0b111111, v, 6, 3000) == true);
    }
    // Disconnected low cell ignored
    {
        uint16_t v[6] = {3700, 3800, 100, 3900, 4000, 4100};
        assert(is_min_vout(0b111011, v, 6, 3000) == false);
    }
}

int main()
{
    test_count_connected_cells();
    test_calculate_per_cell();
    test_is_calibration_required_basics();
    test_is_calibration_required_threshold();
    test_is_calibration_required_ignores_disconnected();
    test_is_calibration_required_no_cells_connected_quirk();
    test_is_calibration_required_single_cell();
    test_is_calibration_required_extremes();
    test_calculate_balance_no_min_cell();
    test_calculate_balance_all_equal();
    test_calculate_balance_picks_higher_cells();
    test_calculate_balance_skips_disconnected();
    test_calculate_balance_strict_gt();
    test_is_max_vout();
    test_is_min_vout();

    std::puts("test_balancer_logic: OK (15 groups)");
    return 0;
}
