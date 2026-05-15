#pragma once

// Pure-logic port of AnalogInputs::calibrateValue() and reverseCalibrateValue()
// from src/core/AnalogInputs.cpp (lines 305-343).
//
// These are the heart of the firmware's ADC -> real-world unit conversion.
// The author left a TODO ("do this with more points") years ago — the
// two-point linear fit is the *documented* limitation behind real-world
// calibration drift, including the Mega 400Wx2 calibration issues
// (upstream issue #196).
//
// The tests in this module pin down current behaviour, including several
// quirks that aren't obvious from reading the original code:
//
//   1. x == 0 short-circuits to 0 regardless of calibration. The curve
//      has a DISCONTINUITY at x=0 unless the line happens to pass through
//      the origin.
//   2. Results are clamped to [0, UINT16_MAX] — no monotonicity check.
//      If p0.y and p1.y are accidentally swapped, the function silently
//      produces an inverted mapping. This is INTENTIONAL: some sensors
//      (NTC thermistors in a voltage-divider) legitimately have a
//      negative slope. UI-level plausibility checks belong in the
//      calibration menu, not here.
//   3. Degenerate calibration (p0.x == p1.x for calibrate, or
//      p0.y == p1.y for reverse) returns 0 as a safe zero. Previously
//      these were division-by-zero UB; guarded in this fork.
//   4. Integer division truncates. Combined with the two-point fit,
//      this means midrange readings on non-linear sensors (high-current
//      shunts on the 400W class chargers) drift several percent from
//      the calibrated endpoints.

#include <cstdint>

namespace cheali_sim {

// One calibration anchor: (ADC reading, real value).
// Two of these define the linear fit.
struct CalibrationPoint {
    uint16_t x;  // raw ADC reading
    uint16_t y;  // corresponding real-world value (mV, mA, etc.)
};

// Port of AnalogInputs::calibrateValue().
//
// y = p0.y + (x - p0.x) * (p1.y - p0.y) / (p1.x - p0.x)
//
// with the firmware's exact short-circuits and clamps:
//   - if x == 0 return 0  (discontinuity at origin, see header)
//   - clamp result to [0, UINT16_MAX]
uint16_t calibrate_value(uint16_t x,
                         const CalibrationPoint& p0,
                         const CalibrationPoint& p1);

// Port of AnalogInputs::reverseCalibrateValue().
// Inverse map: given a real value y, return what ADC reading would produce it.
//
// Same short-circuits as calibrate_value (y == 0 -> 0; clamp).
uint16_t reverse_calibrate_value(uint16_t y,
                                 const CalibrationPoint& p0,
                                 const CalibrationPoint& p1);

}  // namespace cheali_sim
