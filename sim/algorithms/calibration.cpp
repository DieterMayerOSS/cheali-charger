#include "calibration.h"

#include <algorithm>
#include <climits>

namespace cheali_sim {

uint16_t calibrate_value(uint16_t x,
                         const CalibrationPoint& p0,
                         const CalibrationPoint& p1)
{
    if (x == 0) return 0;          // firmware short-circuit (discontinuity at origin)
    if (p0.x == p1.x) return 0;    // degenerate calibration -> safe zero

    const int32_t dy_anchor = int32_t{p1.y} - p0.y;
    const int32_t dx_input  = int32_t{x}    - p0.x;
    const int32_t dx_anchor = int32_t{p1.x} - p0.x;
    const int32_t y = p0.y + dy_anchor * dx_input / dx_anchor;

    return static_cast<uint16_t>(std::clamp<int32_t>(y, 0, UINT16_MAX));
}

uint16_t reverse_calibrate_value(uint16_t y,
                                 const CalibrationPoint& p0,
                                 const CalibrationPoint& p1)
{
    if (y == 0) return 0;
    if (p0.y == p1.y) return 0;    // degenerate calibration -> safe zero

    const int32_t dx_anchor = int32_t{p1.x} - p0.x;
    const int32_t dy_input  = int32_t{y}    - p0.y;
    const int32_t dy_anchor = int32_t{p1.y} - p0.y;
    const int32_t x = p0.x + dx_anchor * dy_input / dy_anchor;

    return static_cast<uint16_t>(std::clamp<int32_t>(x, 0, UINT16_MAX));
}

}  // namespace cheali_sim
