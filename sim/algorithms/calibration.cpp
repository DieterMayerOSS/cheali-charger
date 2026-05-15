#include "calibration.h"

#include <climits>

namespace cheali_sim {

uint16_t calibrate_value(uint16_t x,
                         const CalibrationPoint& p0,
                         const CalibrationPoint& p1)
{
    if (x == 0) return 0;          // firmware short-circuit (discontinuity at origin)
    if (p0.x == p1.x) return 0;    // degenerate calibration -> safe zero

    int32_t y, a;
    y  = p1.y;  y -= p0.y;
    a  = x;     a -= p0.x;
    y *= a;
    a  = p1.x;  a -= p0.x;
    y /= a;
    y += p0.y;

    if (y < 0)         y = 0;
    if (y > UINT16_MAX) y = UINT16_MAX;
    return static_cast<uint16_t>(y);
}

uint16_t reverse_calibrate_value(uint16_t y,
                                 const CalibrationPoint& p0,
                                 const CalibrationPoint& p1)
{
    if (y == 0) return 0;
    if (p0.y == p1.y) return 0;    // degenerate calibration -> safe zero

    int32_t x, a;
    x  = p1.x;  x -= p0.x;
    a  = y;     a -= p0.y;
    x *= a;
    a  = p1.y;  a -= p0.y;
    x /= a;
    x += p0.x;

    if (x < 0)         x = 0;
    if (x > UINT16_MAX) x = UINT16_MAX;
    return static_cast<uint16_t>(x);
}

}  // namespace cheali_sim
