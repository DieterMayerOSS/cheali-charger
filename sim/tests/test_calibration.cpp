#include "calibration.h"

#include <cassert>
#include <climits>
#include <cstdio>
#include <initializer_list>

using cheali_sim::CalibrationPoint;
using cheali_sim::calibrate_value;
using cheali_sim::reverse_calibrate_value;

// ---- calibrate_value ------------------------------------------------------

static void test_calibrate_zero_short_circuits()
{
    // x == 0 always returns 0 regardless of calibration. With p0=(10, 1000),
    // the linear value at x=0 would be ~950, but firmware forces 0.
    CalibrationPoint p0{10, 1000};
    CalibrationPoint p1{100, 4000};
    assert(calibrate_value(0, p0, p1) == 0);
    // At x=1, however, the linear fit is used. With p0=(10, 1000) the
    // value at x=1 is 1000 + (1-10) * (3000/90) = 1000 - 300 = 700.
    // This is the DISCONTINUITY: x=0 -> 0, but x=1 -> 700.
    assert(calibrate_value(1, p0, p1) == 700);
}

static void test_calibrate_hits_anchors()
{
    CalibrationPoint p0{100, 1000};
    CalibrationPoint p1{500, 5000};
    assert(calibrate_value(100, p0, p1) == 1000);
    assert(calibrate_value(500, p0, p1) == 5000);
}

static void test_calibrate_midpoint()
{
    // Linear: x=300 is the midpoint of [100,500], y should be 3000
    CalibrationPoint p0{100, 1000};
    CalibrationPoint p1{500, 5000};
    assert(calibrate_value(300, p0, p1) == 3000);
}

static void test_calibrate_clamps_below_zero()
{
    // Tiny x below p0 where extrapolation goes negative -> clamped to 0
    CalibrationPoint p0{100, 1000};
    CalibrationPoint p1{200, 5000};
    // y = 1000 + (1 - 100) * 4000 / 100 = 1000 - 3960 = -2960 -> 0
    assert(calibrate_value(1, p0, p1) == 0);
}

static void test_calibrate_clamps_to_uint16_max()
{
    // x far above p1 -> overflow region clamped to UINT16_MAX (65535)
    CalibrationPoint p0{0, 0};
    CalibrationPoint p1{1, 1000};   // very steep slope
    // y = 0 + (60000 - 0) * 1000 / 1 = 60_000_000 -> clamped to 65535
    assert(calibrate_value(60000, p0, p1) == UINT16_MAX);
}

static void test_calibrate_inverted_calibration_silently_inverts()
{
    // QUIRK: there is no monotonicity check. If a user accidentally swaps
    // the y values (e.g. calibrates p0=(100, 5000), p1=(500, 1000)), the
    // function silently produces an inverted mapping — high ADC -> low
    // value. No error, no warning.
    CalibrationPoint p0{100, 5000};
    CalibrationPoint p1{500, 1000};
    assert(calibrate_value(300, p0, p1) == 3000);   // midpoint still 3000
    // ...but the endpoints are now inverted from what you'd expect:
    assert(calibrate_value(100, p0, p1) == 5000);  // low ADC -> high value
    assert(calibrate_value(500, p0, p1) == 1000);  // high ADC -> low value
}

static void test_calibrate_integer_truncation_midrange()
{
    // Real-world drift example for the Mega 400Wx2: imagine a current
    // shunt that reads slightly non-linearly. Calibrate endpoints at
    // ADC=100 -> 0mA and ADC=10000 -> 10000mA (10A). The MIDPOINT
    // measurement of ADC=5050 will map linearly to 5000mA, but the
    // true current at that ADC might be 5300mA if the shunt sags.
    // The two-point fit cannot represent that curvature.
    //
    // This test just nails down the linear behaviour; the real-world
    // gap is what motivates a future 3-point upgrade.
    CalibrationPoint p0{100, 0};
    CalibrationPoint p1{10000, 10000};
    // Exact midpoint: (5050-100) * 10000 / 9900 + 0 = 5000
    assert(calibrate_value(5050, p0, p1) == 5000);
    // Off by one: integer truncation. (5051-100) * 10000 / 9900 = 5001
    assert(calibrate_value(5051, p0, p1) == 5001);
}

// ---- reverse_calibrate_value ----------------------------------------------

static void test_reverse_zero_short_circuits()
{
    CalibrationPoint p0{10, 1000};
    CalibrationPoint p1{100, 4000};
    assert(reverse_calibrate_value(0, p0, p1) == 0);
}

static void test_reverse_hits_anchors()
{
    CalibrationPoint p0{100, 1000};
    CalibrationPoint p1{500, 5000};
    assert(reverse_calibrate_value(1000, p0, p1) == 100);
    assert(reverse_calibrate_value(5000, p0, p1) == 500);
}

static void test_round_trip()
{
    // For "round-numbered" inputs, calibrate then reverse should
    // give back the original (modulo integer rounding).
    CalibrationPoint p0{100, 1000};
    CalibrationPoint p1{500, 5000};
    for (uint16_t x : {200, 300, 400, 450, 250, 350}) {
        uint16_t y = calibrate_value(x, p0, p1);
        uint16_t x2 = reverse_calibrate_value(y, p0, p1);
        assert(x2 == x);
    }
}

static void test_reverse_clamps()
{
    // y far above p1.y -> x extrapolates beyond p1.x; clamped at UINT16_MAX
    CalibrationPoint p0{0, 0};
    CalibrationPoint p1{1000, 1};   // ADC reads huge for tiny y
    // x = 0 + (50000 - 0) * 1000 / 1 = 50_000_000 -> UINT16_MAX
    assert(reverse_calibrate_value(50000, p0, p1) == UINT16_MAX);

    // y below p0.y with steep slope -> x negative -> clamped to 0
    CalibrationPoint p2{1000, 5000};
    CalibrationPoint p3{2000, 5100};
    // y = 1 (just above 0): x = 1000 + (1 - 5000) * 1000 / 100 = 1000 - 49990 = -48990 -> 0
    assert(reverse_calibrate_value(1, p2, p3) == 0);
}

int main()
{
    test_calibrate_zero_short_circuits();
    test_calibrate_hits_anchors();
    test_calibrate_midpoint();
    test_calibrate_clamps_below_zero();
    test_calibrate_clamps_to_uint16_max();
    test_calibrate_inverted_calibration_silently_inverts();
    test_calibrate_integer_truncation_midrange();

    test_reverse_zero_short_circuits();
    test_reverse_hits_anchors();
    test_round_trip();
    test_reverse_clamps();

    std::puts("test_calibration: OK (11 cases)");
    return 0;
}
