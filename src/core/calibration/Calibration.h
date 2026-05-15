/*
    cheali-charger - open source firmware for a variety of LiPo chargers
    Copyright (C) 2016  Paweł Stawicki. All right reserved.

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
#ifndef CALIBRATION_H_
#define CALIBRATION_H_

#include "AnalogInputs.h"

/**
 * @brief Per-channel ADC calibration menus and validation.
 *
 * Each calibration sub-menu walks the user through entering the real-world
 * value for a known measurement point. The captured ADC reading and the
 * entered real value are stored as a CalibrationPoint pair (p0, p1) in
 * EEPROM and used at runtime by AnalogInputs::calibrateValue() for linear
 * interpolation.
 */
namespace Calibration {
    /// Voltage calibration menu — input voltage, individual cell pins, and balance port.
    void voltageCalibration();
    /// Charge-current calibration menu (IsmpsSet / Ismps channels).
    void chargeCurrentCalibration();
    /// Discharge-current calibration menu (IdischargeSet / Idischarge channels).
    void dischargeCurrentCalibration();
    /// External temperature calibration menu (Textern). Negative slope allowed (NTC).
    void externalTemperatureCalibration();
    /// Internal temperature calibration menu (Tintern). Negative slope allowed.
    void internalTemperatureCalibration();
    /// Expert-only menu for raw pin calibrations (Vout_plus/minus_pin, Vb0..2_pin).
    void expertVoltageCalibration();

    /**
     * @brief Wait for a battery (and optionally balance port) to be connected.
     * @param balancePort if true, also require the balance port to be plugged in
     * @return true once the connection is detected; false if user cancels (STOP key)
     */
    bool testVout(bool balancePort);

    /**
     * @brief Run the top-level calibration menu loop.
     *
     * Returns when the user exits. Before returning, invokes check() so that
     * inverted or degenerate voltage / current calibrations are reported
     * immediately, not delayed until the next charge program starts.
     */
    void run();

    /**
     * @brief Validate stored calibrations against expected ranges.
     *
     * Checks Vout_plus_pin, charge / discharge current channels, and each
     * individual balance-port pin (Vb1_pin..Vb6_pin) for plausibility.
     * Temperatures are deliberately not checked (negative slope is legitimate).
     *
     * On failure, calls Screen::runCalibrationError() with an error code
     * 1..7 — see Calibration.cpp::check() for the meaning of each code.
     *
     * @return true if all checks pass, false otherwise. When
     *         ENABLE_CALIBRATION_CHECK is undefined, always returns true.
     */
    bool check();
};

#endif /* CALIBRATION_H_ */
