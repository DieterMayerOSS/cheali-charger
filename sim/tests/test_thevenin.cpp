#include "thevenin.h"

#include <cassert>
#include <climits>
#include <cstdio>

using cheali_sim::Resistance;
using cheali_sim::Thevenin;

static void test_resistance_readable()
{
    // uI == 0 -> 0 (avoid div-by-zero)
    {
        Resistance r{100, 0};
        assert(r.getReadableRth() == 0);
    }

    // Positive iV: R = |iV| * 1000 / uI
    // 100 mV / 1000 mA = 0.1 ohm = 100 mOhm
    {
        Resistance r{100, 1000};
        assert(r.getReadableRth() == 100);
    }

    // Negative iV (discharge): absolute value is used
    {
        Resistance r{-100, 1000};
        assert(r.getReadableRth() == 100);
    }
}

static void test_init_charge_normal()
{
    // Charge, Vth < Vmax (normal start)
    Thevenin t;
    t.init(/*Vth=*/3700, /*Vmax=*/4200, /*i=*/1000, /*charge=*/true);

    // Vfrom = min = 3700, Vto = max = 4200
    assert(t.VLast_ == 3700);
    assert(t.Vth_   == 3700);
    assert(t.ILast_ == 0);
    assert(t.ILastDiff_ == 0);
    assert(t.Rth.uI == 1000);
    assert(t.Rth.iV == 500);  // Vto - Vfrom = 4200 - 3700
}

static void test_init_charge_overcharged_safety()
{
    // Charge, but Vth > Vmax (e.g. one cell overcharged).
    // Comment in init(): "safety routine - important when one cell is overcharged"
    Thevenin t;
    t.init(/*Vth=*/4300, /*Vmax=*/4200, /*i=*/1000, /*charge=*/true);

    // Vfrom = min = 4200, Vto = max = 4300 -> Vth_ starts at the lower of the two
    assert(t.Vth_   == 4200);
    assert(t.Rth.iV == 100);
}

static void test_init_discharge()
{
    // Discharge: Vfrom = max, Vto = min, so Rth.iV is negative
    Thevenin t;
    t.init(/*Vth=*/4100, /*Vmax=*/3000, /*i=*/1000, /*charge=*/false);

    assert(t.Vth_   == 4100);  // Vfrom = max(4100, 3000) = 4100
    assert(t.Rth.uI == 1000);
    assert(t.Rth.iV == -1100); // Vto - Vfrom = 3000 - 4100 = -1100
}

static void test_calculate_i_divbyzero()
{
    // Rth.iV == 0 returns UINT16_MAX (signals "no constraint")
    Thevenin t;
    t.init(/*Vth=*/4200, /*Vmax=*/4200, /*i=*/1000, /*charge=*/true);
    assert(t.Rth.iV == 0);
    assert(t.calculateI(4000) == UINT16_MAX);
}

static void test_calculate_i_normal_charge()
{
    // Normal charge: i = (v - Vth_) * Rth.uI / Rth.iV
    // After init(3700, 4200, 1000, true): Vth_=3700, Rth.uI=1000, Rth.iV=500
    // calculateI(4200) = (4200 - 3700) * 1000 / 500 = 1000
    Thevenin t;
    t.init(3700, 4200, 1000, true);
    assert(t.calculateI(4200) == 1000);

    // Halfway: calculateI(3950) = 250 * 1000 / 500 = 500
    assert(t.calculateI(3950) == 500);

    // Below Vth_: result is negative -> clamped to 0
    assert(t.calculateI(3600) == 0);
}

static void test_calculate_i_overflow_clamp()
{
    // Very steep slope -> i > UINT16_MAX -> clamped
    Thevenin t;
    t.init(0, 1, 60000, true);  // Vth_=0, Rth.uI=60000, Rth.iV=1
    // calculateI(2) = 2 * 60000 / 1 = 120000 > 65535 -> UINT16_MAX
    assert(t.calculateI(2) == UINT16_MAX);
}

static void test_calculate_rth_deadband_ignores_small_change()
{
    // calculateRth() only updates if |i - ILast_| > ILastDiff_ / 2
    // After init, ILastDiff_ = 0, so any nonzero change passes the gate.
    // After one successful update, the gate becomes half of that change.
    Thevenin t;
    t.init(3700, 4200, 1000, true);
    int16_t  iV_before = t.Rth.iV;
    uint16_t uI_before = t.Rth.uI;

    // Tiny change (i == ILast_) is below the gate: nothing happens
    t.storeLast(3700, 1000);
    t.calculateRth(3700, 1000);
    assert(t.Rth.iV == iV_before);
    assert(t.Rth.uI == uI_before);
}

static void test_calculate_rth_sign_mismatch_rejected()
{
    // Even if magnitudes are large, an update with wrong sign of rth_v
    // is rejected (sanity check against measurement glitches).
    Thevenin t;
    t.init(3700, 4200, 1000, /*charge=*/true);
    // Rth.iV > 0 in charge mode. If a sample suggests rth_v < 0,
    // sign(rth_v) != sign(Rth.iV), so no update.
    int16_t  iV_before = t.Rth.iV;
    uint16_t uI_before = t.Rth.uI;

    t.storeLast(/*VLast=*/3800, /*ILast=*/1000);
    // Now feed v < VLast_ at i > ILast_: rth_v = v - VLast_ < 0, but Rth.iV > 0
    t.calculateRth(/*v=*/3750, /*i=*/1100);

    assert(t.Rth.iV == iV_before);
    assert(t.Rth.uI == uI_before);
}

static void test_calculate_rth_successful_update()
{
    // Plausible measurement: voltage rises with current rise -> update accepted
    Thevenin t;
    t.init(3700, 4200, 1000, /*charge=*/true);

    t.storeLast(/*VLast=*/3700, /*ILast=*/0);
    // New sample: v=3750, i=500 -> rth_v = 50, rth_i = 500
    t.calculateRth(/*v=*/3750, /*i=*/500);

    assert(t.Rth.iV == 50);
    assert(t.Rth.uI == 500);
    assert(t.ILastDiff_ == 500);
}

static void test_calculate_vth_normal()
{
    // Vth_ = v - i*Rth.iV/Rth.uI    (open-circuit voltage estimate)
    Thevenin t;
    t.init(3700, 4200, 1000, true);  // Rth.iV=500, Rth.uI=1000 -> R = 0.5
    // At v=4000, i=200: VRth = 200*500/1000 = 100 -> Vth_ = 4000-100 = 3900
    t.calculateVth(4000, 200);
    assert(t.Vth_ == 3900);
}

static void test_calculate_vth_clamp_to_zero()
{
    // If v < VRth, Vth_ is clamped to 0 (rather than underflowing).
    Thevenin t;
    t.init(0, 4200, 1000, true);  // Rth.iV=4200, Rth.uI=1000
    // VRth = 200 * 4200 / 1000 = 840. v=500 < 840 -> Vth_ = 0
    t.calculateVth(500, 200);
    assert(t.Vth_ == 0);
}

static void test_charge_endgame_sequence()
{
    // Simulate the tail of a CV charge: current should decrease as terminal
    // voltage approaches Vmax, ending near zero.
    Thevenin t;
    t.init(/*Vth=*/3700, /*Vmax=*/4200, /*i=*/1000, /*charge=*/true);

    // Drive a small charge sequence: rising V, declining I (the firmware
    // updates Rth/Vth from samples, then asks calculateI for the next setpoint).
    t.storeLast(3700, 0);
    t.calculateRthVth(3800, 200);
    t.storeLast(3800, 200);
    t.calculateRthVth(3950, 100);
    t.storeLast(3950, 100);

    // Asking for the current at terminal == Vmax should produce a small,
    // bounded value (we don't pin the exact number — that's what the
    // earlier per-step tests do; this just verifies sanity).
    uint16_t i_at_endV = t.calculateI(4200);
    assert(i_at_endV <= 1000);  // never above the configured limit
}

int main()
{
    test_resistance_readable();
    test_init_charge_normal();
    test_init_charge_overcharged_safety();
    test_init_discharge();
    test_calculate_i_divbyzero();
    test_calculate_i_normal_charge();
    test_calculate_i_overflow_clamp();
    test_calculate_rth_deadband_ignores_small_change();
    test_calculate_rth_sign_mismatch_rejected();
    test_calculate_rth_successful_update();
    test_calculate_vth_normal();
    test_calculate_vth_clamp_to_zero();
    test_charge_endgame_sequence();

    std::puts("test_thevenin: OK (13 cases)");
    return 0;
}
