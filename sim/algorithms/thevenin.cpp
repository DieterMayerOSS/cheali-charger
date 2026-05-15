#include "thevenin.h"

#include <cstdlib>
#include <climits>

namespace cheali_sim {

namespace {

template <class T>
T absDiff(T x, T y) {
    return x > y ? x - y : y - x;
}

int8_t sign(int16_t x) {
    if (x == 0) return 0;
    if (x >  0) return 1;
    return -1;
}

template <class T>
T min_(T a, T b) { return a < b ? a : b; }
template <class T>
T max_(T a, T b) { return a > b ? a : b; }

}  // namespace

uint16_t Resistance::getReadableRth() const
{
    if (uI == 0) return 0;
    uint32_t R = std::abs(static_cast<int32_t>(iV));
    R *= ANALOG_VOLT_1_0;
    R /= uI;
    return static_cast<uint16_t>(R);
}

void Thevenin::init(uint16_t Vth, uint16_t Vmax, uint16_t i, bool charge)
{
    uint16_t Vfrom, Vto;
    if (charge) {
        // safety routine - important when one cell is overcharged
        Vfrom = min_(Vth, Vmax);
        Vto   = max_(Vth, Vmax);
    } else {
        Vfrom = max_(Vth, Vmax);
        Vto   = min_(Vth, Vmax);
    }
    VLast_ = Vth_ = Vfrom;
    ILastDiff_ = ILast_ = 0;

    Rth.uI = i;
    Rth.iV = static_cast<int16_t>(Vto) - static_cast<int16_t>(Vfrom);
}

uint16_t Thevenin::calculateI(uint16_t v) const
{
    int32_t i;
    i  = v;
    i -= Vth_;
    i *= Rth.uI;
    if (Rth.iV == 0) return UINT16_MAX;
    i /= Rth.iV;
    if (i > UINT16_MAX) return UINT16_MAX;
    if (i < 0) return 0;
    return static_cast<uint16_t>(i);
}

void Thevenin::calculateRthVth(uint16_t v, uint16_t i)
{
    calculateRth(v, i);
    calculateVth(v, i);
}

void Thevenin::calculateRth(uint16_t v, uint16_t i)
{
    if (absDiff(i, ILast_) > ILastDiff_ / 2) {
        int16_t  rth_v;
        uint16_t rth_i;
        if (i > ILast_) {
            rth_i  = i;
            rth_i -= ILast_;
            rth_v  = static_cast<int16_t>(v);
            rth_v -= static_cast<int16_t>(VLast_);
        } else {
            rth_v  = static_cast<int16_t>(VLast_);
            rth_v -= static_cast<int16_t>(v);
            rth_i  = ILast_;
            rth_i -= i;
        }
        if (sign(rth_v) == sign(Rth.iV)) {
            ILastDiff_ = rth_i;
            Rth.iV = rth_v;
            Rth.uI = rth_i;
        }
    }
}

void Thevenin::calculateVth(uint16_t v, uint16_t i)
{
    int32_t VRth;
    VRth  = i;
    VRth *= Rth.iV;
    VRth /= Rth.uI;
    if (static_cast<int32_t>(v) < VRth) Vth_ = 0;
    else Vth_ = static_cast<uint16_t>(v - VRth);
}

}  // namespace cheali_sim
