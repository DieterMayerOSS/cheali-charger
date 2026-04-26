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
#include "TheveninChargeStrategy.h"
#include "DelayStrategy.h"
#include "ProgramData.h"
#include "memory.h"

/*
 * PbFloatStrategy: auto-restarting float charge for Pb/AGM batteries.
 *
 * State machine:
 *   Charge  - CCCV at float voltage (VStorage) via TheveninChargeStrategy.
 *             On COMPLETE (current fell below minIc): transition to Wait.
 *             On ERROR: propagate.
 *   Wait    - Idle for DCRestTime minutes via DelayStrategy.
 *             On COMPLETE: transition back to Charge.
 *             On ERROR: propagate.
 *
 * The loop never returns COMPLETE on its own; only STOP (user key) or
 * ERROR will exit it.
 */

namespace PbFloatStrategy {

    enum State { Charge, Wait };
    State state_;

    const Strategy::VTable vtable PROGMEM = {
        powerOn,
        powerOff,
        doStrategy
    };

    void startCharge() {
        Strategy::setVI(ProgramData::VStorage, true);
        TheveninChargeStrategy::powerOn();
        state_ = Charge;
    }

    void startWait() {
        DelayStrategy::setDelay(ProgramData::battery.DCRestTime);
        DelayStrategy::powerOn();
        state_ = Wait;
    }

} // namespace PbFloatStrategy


void PbFloatStrategy::powerOn()
{
    startCharge();
}

void PbFloatStrategy::powerOff()
{
    TheveninChargeStrategy::powerOff();
    DelayStrategy::powerOff();
}

Strategy::statusType PbFloatStrategy::doStrategy()
{
    Strategy::statusType status;

    switch(state_) {
        case Charge:
            status = TheveninChargeStrategy::doStrategy();
            if(status == Strategy::COMPLETE) {
                TheveninChargeStrategy::powerOff();
                startWait();
                return Strategy::RUNNING;
            }
            return status;

        default: // Wait
            status = DelayStrategy::doStrategy();
            if(status == Strategy::COMPLETE) {
                startCharge();
                return Strategy::RUNNING;
            }
            return status;
    }
}
