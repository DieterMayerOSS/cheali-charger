/*
    cheali-charger - open source firmware for a variety of LiPo chargers
    Copyright (C) 2013  Paweł Stawicki. All right reserved.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef ANALOGINPUTS_H_
#define ANALOGINPUTS_H_

#include "AnalogInputsTypes.h"
#include "HardwareConfig.h"
#include "cpu/config.h"

#define ANALOG_INPUTS_MAX_CALIBRATION_POINTS    2
#define ANALOG_INPUTS_DELTA_TIME_MILISECONDS    30000
#define ANALOG_INPUTS_RESOLUTION                16  // bits

#define ANALOG_INPUTS_MAX_ADC_VALUE      (((1<<(ANALOG_INPUTS_ADC_RESOLUTION_BITS))-1) << ((ANALOG_INPUTS_RESOLUTION) - (ANALOG_INPUTS_ADC_RESOLUTION_BITS)))

#define ANALOG_INPUTS_FOR_ALL_PHY(iterator) for(AnalogInputs::Name iterator = AnalogInputs::Name(0); iterator < AnalogInputs::PHYSICAL_INPUTS; iterator = AnalogInputs::Name(iterator + 1) )
#define ANALOG_INPUTS_FOR_ALL(iterator)     for(AnalogInputs::Name iterator = AnalogInputs::Name(0); iterator < AnalogInputs::ALL_INPUTS;      iterator = AnalogInputs::Name(iterator + 1) )

/**
 * @brief ADC channel inventory, calibration, and measurement plumbing.
 *
 * Wraps the AVR ADC behind a uniform Name-keyed interface. Per-channel
 * raw ADC readings flow through a 2-point linear calibration into "real"
 * values (mV, mA, mC, etc.) consumed by the rest of the firmware.
 *
 * Channels split into:
 *  - PHYSICAL_INPUTS: actual ADC pins, calibrated, persisted to EEPROM
 *  - virtual inputs:  derived values (Vout, Iout, Vbalancer, delta-Vout, ...)
 *  - cell readings:   Vb1..Vb6 — per-cell voltages, computed from cell pins
 */
namespace AnalogInputs {

    /// Single calibration anchor: a raw ADC reading (x) paired with the
    /// corresponding real-world value (y). Linear interpolation between
    /// two of these defines the calibration line. EEPROM-packed.
    struct CalibrationPoint {
        ValueType x;  ///< raw ADC reading
        ValueType y;  ///< real-world value (mV/mA/mC/etc., scale per channel)
    } CHEALI_EEPROM_PACKED;

    /// Per-channel factory default — two anchors. Used by restoreDefault()
    /// to seed EEPROM after CRC mismatch.
    struct DefaultValues {
        CalibrationPoint p0;
        CalibrationPoint p1;
    } CHEALI_EEPROM_PACKED;

    /// In-EEPROM calibration record. The array holds
    /// ANALOG_INPUTS_MAX_CALIBRATION_POINTS (= 2) points per channel.
    struct Calibration {
        CalibrationPoint p[ANALOG_INPUTS_MAX_CALIBRATION_POINTS];
    } CHEALI_EEPROM_PACKED;

    /**
     * @brief Channel identifier for every measurement the firmware knows about.
     *
     * Ordering matters: enumerators before VirtualInputs are physical ADC
     * channels (PHYSICAL_INPUTS count), enumerators after VirtualInputs
     * are derived. EEPROM calibration storage is sized by PHYSICAL_INPUTS,
     * so inserting a new physical channel breaks the EEPROM layout — bump
     * cheali-charger-eeprom-calibration-version in CMakeLists.txt.
     */
    enum Name {
        Vout_plus_pin,     ///< raw ADC on +output rail
        Vout_minus_pin,    ///< raw ADC on -output rail
        Ismps,             ///< SMPS-side charge current
        Idischarge,        ///< discharge current

        VoutMux,
        Tintern,           ///< internal (PCB / heatsink) temperature
        Vin,               ///< supply input voltage
        Textern,           ///< external (battery) temperature probe

        Vb0_pin,           ///< balance port reference (factory-uncalibrated; do not check())
        Vb1_pin,
        Vb2_pin,
        Vb3_pin,
        Vb4_pin,
        Vb5_pin,
        Vb6_pin,

#if MAX_BALANCE_CELLS > 6
        Vb7_pin,
        Vb8_pin,
#endif

        IsmpsSet,          ///< charge current setpoint (DAC feedback path)
        IdischargeSet,     ///< discharge current setpoint

        VirtualInputs,     ///< marker — everything below is computed, not measured
        Vout,
        Vbalancer,
        VoutBalancer,      ///< Vbalancer if balance port connected, else Vout
        VobInfo,
        VbalanceInfo,

        Iout,
        Pout,              ///< instantaneous power (V*I)
        Cout,              ///< accumulated charge (Ah, integer-scaled)
        Eout,              ///< accumulated energy (Wh, integer-scaled)

        deltaVout,         ///< short-term dV used for NiMH/NiCd -dV termination
        deltaVoutMax,
        deltaTextern,      ///< short-term dT used for delta-T termination
        deltaLastCount,

        Vb1,               ///< per-cell voltage (Vb1_pin minus lower cells)
        Vb2,
        Vb3,
        Vb4,
        Vb5,
        Vb6,

#if MAX_BALANCE_CELLS > 6
        Vb7,
        Vb8,
#endif

        LastInput,
    };
    /// Number of physical (calibrated, EEPROM-persisted) channels.
    static const uint8_t    PHYSICAL_INPUTS     = VirtualInputs - Vout_plus_pin;
    /// Total channel count including virtual / derived inputs.
    static const uint8_t    ALL_INPUTS          = LastInput - Vout_plus_pin;
    /// Below this Vout_minus reading the polarity-check considers the
    /// battery reversed (1.0 V on the wrong rail = clearly miswired).
    static const ValueType  REVERSE_POLARITY_MIN_VOLTAGE = ANALOG_VOLT(1.000);
    /// Below this voltage on a balance pin / output, the channel is
    /// considered "no battery connected" — drives isConnected().
    static const ValueType  CONNECTED_MIN_VOLTAGE = ANALOG_VOLT(0.400);

    /**
     * @brief Time-averaged raw ADC value for a physical channel.
     *
     * Smoothed over ANALOG_INPUTS_ADC_MEASUREMENTS_COUNT samples. Use this
     * for calibration entry where you want the steady value, not a single
     * instantaneous reading.
     */
    ValueType getAvrADCValue(Name name);

    /**
     * @brief Calibrated, averaged value for any channel.
     *
     * This is the value you almost always want: raw ADC averaged, then
     * passed through 2-point calibration into real units (mV, mA, mC).
     */
    ValueType getRealValue(Name name);

    /**
     * @brief Last raw ADC sample, atomic with respect to the ADC ISR.
     *
     * Not averaged. Useful when you need a snapshot of the current
     * conversion (e.g. fast safety checks in Monitor::run).
     */
    ValueType getADCValue(Name name);

    /**
     * @brief Pack voltage — prefers the balance port reading when connected.
     *
     * Returns VoutBalancer if the balance port is plugged in (more accurate),
     * otherwise falls back to Vout.
     */
    ValueType getVbattery();
    /// Output terminal voltage (no balance-port fallback).
    ValueType getVout();
    /// Output current.
    ValueType getIout();
    ValueType getDeltaLastT();
    ValueType getDeltaCount();
    /// Accumulated charge in mAh (integer-scaled).
    ValueType getCharge();
    /// Accumulated energy in mWh (integer-scaled). Computed on a slower
    /// cadence than getCharge() — sampled every ANALOG_INPUTS_E_OUT_dt_FACTOR
    /// ticks rather than per-tick.
    ValueType getEout();
    void enableDeltaVoutMax(bool enable);

    extern uint16_t connectedBalancePortCells;
    uint8_t getConnectedBalancePortCellsCount();
    void saveBalancePortState();

    uint16_t getFullMeasurementCount();
    uint16_t getStableCount(Name name);

    Type getType(Name name);

    bool isOutStable();
    bool isStable(Name name);

    /**
     * @brief Whether a channel currently sees a real signal.
     *
     * For voltage channels: returns true iff getRealValue(name) exceeds
     * CONNECTED_MIN_VOLTAGE (0.4 V). For Vbalancer specifically, checks
     * the VobInfo discriminator. Non-voltage channels always return true.
     */
    bool isConnected(Name name);

    /// Whether the balance port is plugged in (delegates to isConnected(Vbalancer)).
    bool isBalancePortConnected();

    /**
     * @brief Whether the user has connected the battery with reversed polarity.
     *
     * On chargers with OUTPUT_VOLTAGE_MINUS_PIN: derived from comparing
     * Vout_minus_pin against Vout_plus_pin and REVERSE_POLARITY_MIN_VOLTAGE.
     * Otherwise: delegated to a hardware-specific check.
     */
    bool isReversePolarity();
    bool isPowerOn();

    void doFullMeasurement();

    void resetMeasurement();
    void resetAccumulatedMeasurements();

    /**
     * @brief Enable measurement and (optionally) the battery output relay.
     * @param enableBatteryOutput drive the battery output relay on (default: true).
     *        Pass false to measure with the battery hard-disconnected
     *        (e.g. during voltage calibration where the relay would skew readings).
     */
    void powerOn(bool enableBatteryOutput = true);

    /// Disable measurement and the battery output relay.
    void powerOff();


// calibration

    /// Restore factory default calibration points from PROGMEM (inputsP_)
    /// and refresh the EEPROM calibration CRC.
    void restoreDefault();

    /**
     * @brief Map a raw ADC reading to its calibrated real-world value.
     *
     * Linear interpolation between the two stored CalibrationPoints for
     * @p name.
     *
     * @param name channel to look up calibration for
     * @param x raw ADC value (use 0 to mean "no signal" — short-circuits to 0)
     * @return calibrated value, clamped to [0, UINT16_MAX]
     *
     * @note Returns 0 when calibration is degenerate (p0.x == p1.x) instead
     *       of dividing by zero. No monotonicity check — negative slope is
     *       allowed (NTC thermistors). See Calibration::check() for the
     *       monotonicity validation that runs at higher level.
     */
    ValueType calibrateValue(Name name, ValueType x);

    /// Inverse of calibrateValue(): given a real value, return what raw ADC
    /// reading would produce it. Same edge-case behaviour
    /// (y == 0 → 0, p0.y == p1.y → 0, clamp).
    ValueType reverseCalibrateValue(Name name, ValueType y);

// init

    void initialize();

    void printRealValue(Name name, uint8_t dig);
};

template<class T>
inline T absDiff(T x, T y)
{
    if(x > y) return x - y;
    return y - x;
}


#endif /* ANALOGINPUTS_H_ */
