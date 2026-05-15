#include "calibration.h"

#include <climits>

namespace cheali_sim {

uint16_t calibrate_value(uint16_t x,
                         const CalibrationPoint& p0,
                         const CalibrationPoint& p1)
{
    if (x == 0) return 0;  // firmware short-circuit (discontinuity at origin)

    int32_t y, a;
    y  = p1.y;  y -= p0.y;
    a  = x;     a -= p0.x;
    y *= a;
    a  = p1.x;  a -= p0.x;
    y /= a;                // NB: UB if p1.x == p0.x — firmware has no guard
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

    int32_t x, a;
    x  = p1.x;  x -= p0.x;
    a  = y;     a -= p0.y;
    x *= a;
    a  = p1.y;  a -= p0.y;
    x /= a;                // UB if p1.y == p0.y
    x += p0.x;

    if (x < 0)         x = 0;
    if (x > UINT16_MAX) x = UINT16_MAX;
    return static_cast<uint16_t>(x);
}

}  // namespace cheali_sim
