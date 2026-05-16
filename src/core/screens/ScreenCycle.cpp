/*
    cheali-charger - open source firmware for a variety of LiPo chargers
    Copyright (C) 2013  Pawel Stawicki. All right reserved.

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
#include "memory.h"
#include "Screen.h"
#include "LcdPrint.h"
#include "ProgramData.h"
#include "TheveninMethod.h"
#include "Settings.h"
#include "Hardware.h"
#include "Program.h"
#include "DelayStrategy.h"
#include "Version.h"
#include "ProgramDCcycle.h"
#include "Monitor.h"
#include "PolarityCheck.h"
#include "Keyboard.h"
#include "ScreenCycle.h"

//TODO: remove constant: 10 (move to #define)

namespace Screen { namespace Cycle {

    //TODO_NJ for cyclehistory
    uint16_t cyclesHistoryCapacity[10];
    uint32_t cyclesHistoryTime[10];

} // namespace Screen
} // namespace Cycle

void Screen::Cycle::displayCycles()
{
    // Manual vs auto scrolling. The static state survives across page
    // navigation, so the user can leave the page and return to the same
    // cycle they were viewing. Long-press of START while on this page
    // toggles the mode; in manual mode Inc/Dec scrolls cycles (and the
    // events are consumed so they don't also advance pageNr_); the
    // cycle digit blinks to indicate manual mode is active.
    static uint8_t c = 0;
    static bool manualMode = false;

    const uint8_t time     = Blink::blinkTime_ / 8;
    const uint8_t lastCycleIdx = ProgramDCcycle::currentCycle / 2;
    const uint8_t all_scr  = lastCycleIdx + 1;

    if(Screen::keyboardButton == BUTTON_START && Keyboard::isLongPressTime()) {
        manualMode = !manualMode;
        Screen::keyboardButton = BUTTON_NONE;
    }

    if(manualMode) {
        if(Screen::keyboardButton == BUTTON_DEC && c > 0) {
            c--;
            Screen::keyboardButton = BUTTON_NONE;
        } else if(Screen::keyboardButton == BUTTON_INC && c < lastCycleIdx) {
            c++;
            Screen::keyboardButton = BUTTON_NONE;
        }
        if(c > lastCycleIdx) c = lastCycleIdx;
    } else {
        c = time % all_scr;
    }

    lcdSetCursor0_0();
    if(manualMode && (Blink::blinkTime_ & 0x10)) {
        lcdPrintChar(' ');
    } else {
        lcdPrintUnsigned(c+1, 1);
    }
    lcdPrintChar(SCREEN_EMPTY_CELL_CHAR);
    lcdPrintTime(cyclesHistoryTime[c*2], 6);
    lcdPrintSpace1();
    lcdPrintChar(SCREEN_FULL_CELL_CHAR);
    lcdPrintTime(cyclesHistoryTime[c*2+1], 6);
    lcdPrintSpaces();

    lcdSetCursor0_1();
    lcdPrintCharge(cyclesHistoryCapacity[c*2],8);
    lcdPrintCharge(cyclesHistoryCapacity[c*2+1],8);
    lcdPrintSpaces();
}

void Screen::Cycle::resetCycleHistory()
{
    for (uint8_t i = 0; i < 10; i++) {
        cyclesHistoryTime[i] = 0;
        cyclesHistoryCapacity[i] = 0;
    }
}

void Screen::Cycle::storeCycleHistoryInfo()
{
    uint8_t c = ProgramDCcycle::currentCycle;
    cyclesHistoryTime[c] = Monitor::getTotalChargeDischargeTimeSec();
    cyclesHistoryCapacity[c] = AnalogInputs::getRealValue(AnalogInputs::Cout);
}
