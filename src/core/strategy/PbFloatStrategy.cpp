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
#include "PbFloatStrategy.h"
#include "DelayStrategy.h"
#include "ProgramData.h"
#include "AnalogInputs.h"
#include "Hardware.h"
#include "SMPS.h"
#include "Time.h"
#include "Program.h"
#include "memory.h"

/*
 * PbFloatStrategy: self-contained CC/CV float charge for Pb/AGM batteries.
 *
 * State machine:
 *   CC        - Constant-current at maxI until Vbat reaches endV (VStorage).
 *   StabPause - SMPS off for FLOAT_STAB_SECONDS to let voltage settle.
 *               Pb/AGM surface charge dissipates during this pause so CV
 *               starts from a true open-circuit voltage.
 *   CV        - Simple I-controller: every stable ADC sample, step current
 *               up by CV_STEP if Vbat < endV, down by CV_STEP if Vbat > endV.
 *               Ends when I == minI AND Vbat >= endV for CV_DONE_COUNT
 *               consecutive samples, then goes to RestWait.
 *   RestWait  - SMPS off for DCRestTime minutes, then returns to CC.
 *               Always returning to CC guarantees that every CC→CV
 *               transition passes through StabPause.
 *
 * Safety:
 *   - Hardware cutoff: setVoutCutoff(endV + FLOAT_OV_MARGIN) is armed when
 *     CV starts.  The SMPS PID interrupt cuts power in hardware before the
 *     firmware even gets a chance to react.
 *   - Software cutoff: every CV call checks Vbat; if it exceeds
 *     endV + FLOAT_OV_MARGIN the strategy returns ERROR immediately.
 *
 * The loop never returns COMPLETE on its own; only STOP (user key) or
 * ERROR (from Monitor/OV check) will exit it.
 */

/* Stabilisation pause between CC and CV (seconds).
 * 10 s is enough for Pb/AGM surface charge to drain to <5 mV error. */
#define FLOAT_STAB_SECONDS  10u

/*
 * Overvoltage safety margin per cell, scaled by battery.cells at runtime.
 * Total cutoff = endV + FLOAT_OV_MARGIN_PER_CELL * cells.
 *
 * Pb/AGM gassing threshold: ~2.35 V/cell.
 * Typical float (VStorage): ~2.25–2.28 V/cell.
 * Safe window per cell: ~70–100 mV.
 *
 * 50 mV/cell keeps the cutoff well below gassing for all cell counts:
 *   3 cells @ 6.84 V float  → cutoff 7.00 V  (gassing ~7.05 V)
 *   6 cells @ 13.65 V float → cutoff 13.95 V (gassing ~14.10 V) */
#define FLOAT_OV_MARGIN_PER_CELL    ANALOG_VOLT(0.050)

/*
 * CV phase I-controller parameters.
 *
 * Dead zone: [endV - CV_HOLD_WINDOW, endV]  — asymmetric, sits below target.
 *
 *   Vbat > endV               →  step down fast (CV_STEP_DOWN)
 *   endV - WINDOW ≤ Vbat ≤ endV →  hold current unchanged (dead zone)
 *   Vbat < endV - WINDOW      →  step up slow  (CV_STEP_UP)
 *
 * Battery naturally settles just below endV, which is ideal for Pb/AGM
 * float — no oscillation, no repeated OV trips.
 *
 * Step size is proportional to the distance from the float floor (same formula
 * for both directions):
 *   step = (cv_I_ - cv_floatMinI_) / CV_STEP_DIVISOR,  min 1 mA
 * Large steps when current is far above the floor, tiny steps when near it.
 */
#define CV_HOLD_WINDOW    ANALOG_VOLT(0.020)  /* 20 mV dead zone below endV */
#define CV_STEP_DIVISOR   10u                 /* proportional step fraction  */

/* Number of consecutive stable samples at minI (with Vbat >= endV)
 * required before declaring CV complete and entering RestWait. */
#define CV_DONE_COUNT       5u

namespace PbFloatStrategy {

    enum State { CC, StabPause, CV, RestWait };
    State state_;

    uint16_t pauseStart_;

    /* CV I-controller state */
    AnalogInputs::ValueType cv_I_;        /* current setpoint              */
    AnalogInputs::ValueType cv_ovLimit_;  /* endV + cells * margin, cached */
    AnalogInputs::ValueType cv_floatMinI_;/* minI/10 — true float floor    */
    uint8_t                 cv_doneCount_;
    uint16_t                cv_lastStepMs_; /* timestamp of last down-step  */

    const Strategy::VTable vtable PROGMEM = {
        powerOn,
        powerOff,
        doStrategy
    };

    void startCC() {
        Strategy::setVI(ProgramData::VStorage, true);
        SMPS::powerOn();
        state_ = CC;
    }

    void startStabPause() {
        SMPS::powerOff();
        pauseStart_ = Time::getSecondsU16();
        state_ = StabPause;
    }

    void startCV() {
        /* Per-cell OV limit: scale margin by the number of cells so the
         * cutoff tracks the actual battery pack voltage. */
        cv_ovLimit_ = Strategy::endV
                      + (AnalogInputs::ValueType)FLOAT_OV_MARGIN_PER_CELL
                        * ProgramData::battery.cells;

        /* Float minimum: 1/10 of the programme minI, never below 10 mA.
         * A Pb/AGM battery in float only needs a tiny maintenance current;
         * using the full minI (e.g. 100 mA) would keep significant charge
         * current flowing and prevent the termination condition firing. */
        cv_floatMinI_ = Strategy::minI / 10;
        if (cv_floatMinI_ < ANALOG_AMP(0.010))
            cv_floatMinI_ = ANALOG_AMP(0.010);

        /* Arm hardware overvoltage cutoff BEFORE enabling the SMPS.
         * The PID interrupt will cut power if Vout exceeds this threshold,
         * even if firmware is busy elsewhere. */
        hardware::setVoutCutoff(cv_ovLimit_);

        /* Start from float minimum — the battery is at or above endV after
         * the StabPause, so 10 mA is the right entry point.  The controller
         * ramps up only if Vbat drops below the hold window. */
        cv_I_ = cv_floatMinI_;
        cv_doneCount_ = 0;
        cv_lastStepMs_ = Time::getMilisecondsU16();
        SMPS::powerOn();
        SMPS::trySetIout(cv_I_);
        state_ = CV;
    }

    void startRestWait() {
        SMPS::powerOff();
        DelayStrategy::setDelay(ProgramData::battery.DCRestTime);
        DelayStrategy::powerOn();
        state_ = RestWait;
    }

} // namespace PbFloatStrategy


void PbFloatStrategy::powerOn()
{
    startCC();
}

void PbFloatStrategy::powerOff()
{
    SMPS::powerOff();
    DelayStrategy::powerOff();
}

bool PbFloatStrategy::isCC()                    { return state_ == CC; }
bool PbFloatStrategy::isStabPause()             { return state_ == StabPause; }
bool PbFloatStrategy::isCV()                    { return state_ == CV; }
bool PbFloatStrategy::isRestWait()              { return state_ == RestWait; }

uint16_t PbFloatStrategy::getStabRemainingSeconds()
{
    uint16_t elapsed = Time::diffU16(pauseStart_, Time::getSecondsU16());
    if (elapsed >= FLOAT_STAB_SECONDS) return 0;
    return FLOAT_STAB_SECONDS - elapsed;
}

Strategy::statusType PbFloatStrategy::doStrategy(){
    switch (state_) {

        case CC: {
            SMPS::trySetIout(Strategy::maxI);
            if (AnalogInputs::getVbattery() >= Strategy::endV) {
                startStabPause();
            }
            return Strategy::RUNNING;
        }

        case StabPause: {
            if (Time::diffU16(pauseStart_, Time::getSecondsU16()) >= FLOAT_STAB_SECONDS) {
                startCV();
            }
            return Strategy::RUNNING;
        }

        case CV: {
            AnalogInputs::ValueType Vbat = AnalogInputs::getVbattery();

            /* Software overvoltage guard: cut immediately if Vbat escapes
             * the safe window, even before the next ADC stable sample.
             * The hardware cutoff (armed in startCV) provides a second
             * independent layer of protection via the PID interrupt. */
            if (Vbat > cv_ovLimit_) {
                SMPS::powerOff();
                Program::stopReason = Monitor::string_outputCurrentToHigh;
                return Strategy::ERROR;
            }

            /* Down-step: time-gated at ~300 ms, NO stability requirement.
             * When voltage is rising (unstable ADC), isOutStable() never
             * fires, so we must use a timer instead.  300 ms gives the SMPS
             * and battery time to respond between steps without over-reacting. */
            if (Vbat > Strategy::endV) {
                uint16_t now = Time::getMilisecondsU16();
                if (Time::diffU16(cv_lastStepMs_, now) >= 300u) {
                    cv_lastStepMs_ = now;
                    cv_doneCount_ = 0;
                    /* Proportional down-step: big when far above floor, tiny when close */
                    AnalogInputs::ValueType step = (cv_I_ > cv_floatMinI_)
                        ? (cv_I_ - cv_floatMinI_) / CV_STEP_DIVISOR : 0;
                    if (step < 1) step = 1;
                    if (cv_I_ >= cv_floatMinI_ + step)
                        cv_I_ -= step;
                    else
                        cv_I_ = cv_floatMinI_;
                    SMPS::trySetIout(cv_I_);
                }
            } else if (AnalogInputs::isOutStable()) {
                /* Up-step and termination: gated on ADC stability (no rush) */
                if (Vbat < Strategy::endV - CV_HOLD_WINDOW) {
                    /* Proportional up-step: same formula as down-step — distance from floor */
                    cv_lastStepMs_ = Time::getMilisecondsU16();
                    cv_doneCount_ = 0;
                    AnalogInputs::ValueType step = (cv_I_ > cv_floatMinI_)
                        ? (cv_I_ - cv_floatMinI_) / CV_STEP_DIVISOR : 0;
                    if (step < 1) step = 1;
                    if (cv_I_ + step <= Strategy::maxI)
                        cv_I_ += step;
                    else
                        cv_I_ = Strategy::maxI;
                    SMPS::trySetIout(cv_I_);
                } else {
                    /* Dead zone [endV-WINDOW, endV]: hold current, check termination */
                    if (cv_I_ <= cv_floatMinI_) {
                        if (++cv_doneCount_ >= CV_DONE_COUNT) {
                            startRestWait();
                            return Strategy::RUNNING;
                        }
                    } else {
                        cv_doneCount_ = 0;
                    }
                }
            }
            return Strategy::RUNNING;
        }

        default: /* RestWait */ {
            Strategy::statusType status = DelayStrategy::doStrategy();
            if (status == Strategy::COMPLETE) {
                DelayStrategy::powerOff();
                /* Return to CC so that every CC→CV transition always
                 * passes through the StabPause. */
                startCC();
                return Strategy::RUNNING;
            }
            return status;
        }
    }
}
