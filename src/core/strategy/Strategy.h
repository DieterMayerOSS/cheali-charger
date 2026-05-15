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
#ifndef STRATEGY_H_
#define STRATEGY_H_

#include <stdint.h>
#include "AnalogInputs.h"
#include "ProgramData.h"

/**
 * @brief Strategy-pattern dispatcher for charge / discharge / balance / storage / etc.
 *
 * Each concrete strategy (SimpleChargeStrategy, TheveninChargeStrategy,
 * DeltaChargeStrategy, Balancer, Discharger, StorageStrategy, ...) provides
 * a VTable kept in flash (PROGMEM) and the active strategy pointer is
 * stored in Strategy::strategy.
 *
 * The strategy reads its per-run parameters (target voltage, current
 * limits, balance flag) from the namespace-scope variables endV, maxI,
 * minI, doBalance — these are set up by Strategy::setVI() before
 * Strategy::doStrategy() is invoked.
 */
namespace Strategy {
    /// Result of one strategy cycle. RUNNING = continue; COMPLETE = success
    /// (e.g. capacity reached); ERROR = abort (e.g. safety check failed).
    enum statusType {ERROR, COMPLETE, RUNNING };

    /// Function-pointer dispatch table for a concrete strategy.
    /// Stored in PROGMEM (Flash) to save the ~6 bytes of RAM that
    /// a virtual class would otherwise need on AVR.
    struct VTable {
        void (*powerOn)();                ///< invoked when this strategy is selected
        void (*powerOff)();               ///< invoked when this strategy ends or is replaced
        statusType (*doStrategy)();       ///< called every cycle; returns status
    };

    extern AnalogInputs::ValueType endV;  ///< target voltage for the current run
    extern AnalogInputs::ValueType maxI;  ///< upper current limit
    extern AnalogInputs::ValueType minI;  ///< termination threshold (charge ends when I drops below this)
    extern bool doBalance;                ///< whether the balancer should run alongside

    /**
     * @brief Populate endV / maxI / minI from a ProgramData voltage type.
     * @param vt    which voltage profile to use (e.g. VCharged, VDischarged)
     * @param charge true to load charge limits (Ic / minIc), false for discharge
     */
    void setVI(ProgramData::VoltageType vt, bool charge);

    extern const VTable * strategy;       ///< currently active strategy (PROGMEM ptr)
    extern bool exitImmediately;          ///< if true, return without waiting on COMPLETE

    /**
     * @brief Main strategy event loop.
     *
     * Powers on the active strategy, then drives keyboard polling, screen
     * updates, monitor safety checks and strategy-specific logic until
     * COMPLETE / ERROR or the user presses STOP. Powers off on exit.
     */
    statusType doStrategy();
};


#endif /* STRATEGY_H_ */
