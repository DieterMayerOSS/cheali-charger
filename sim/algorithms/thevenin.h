#pragma once

// Pure C++ port of cheali-charger's Thevenin and Resistance classes
// (src/core/strategy/Thevenin.{h,cpp}).
//
// Verbatim port of the math — no behavioural changes. The goal is to
// pin down current firmware behaviour with tests before refactoring.
//
// Units:
//   Voltages   - uint16_t, ANALOG_VOLT(1.0) == 1000 (i.e. millivolt)
//   Currents   - uint16_t, ANALOG_AMP(1.0)  == 1000 (i.e. milliamp)
//   Resistance - the "readable" Rth is returned in milliohm
//                (|iV| in mV * 1000 / uI in mA)

#include <cstdint>

namespace cheali_sim {

constexpr uint16_t ANALOG_VOLT_1_0 = 1000;

class Resistance {
public:
    // R = iV/uI. When discharging the resistance is "negative" (iV < 0):
    // in a Thevenin model that's mathematically equivalent to a positive
    // resistance with the current direction reversed.
    int16_t  iV;
    uint16_t uI;

    uint16_t getReadableRth() const;
};

class Thevenin {
public:
    uint16_t VLast_;
    uint16_t ILast_;
    uint16_t ILastDiff_;
    uint16_t Vth_;
    Resistance Rth;

    Thevenin() = default;

    void storeLast(uint16_t VLast, uint16_t ILast) {
        VLast_ = VLast;
        ILast_ = ILast;
    }

    void init(uint16_t Vth, uint16_t Vmax, uint16_t i, bool charge);

    void calculateRthVth(uint16_t v, uint16_t i);
    void calculateRth(uint16_t v, uint16_t i);
    void calculateVth(uint16_t v, uint16_t i);

    uint16_t calculateI(uint16_t v) const;
};

}  // namespace cheali_sim
