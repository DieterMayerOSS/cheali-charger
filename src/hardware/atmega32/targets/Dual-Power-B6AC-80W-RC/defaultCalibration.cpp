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

#include "AnalogInputsPrivate.h"
#include "memory.h"
#include "Utils.h"

const AnalogInputs::DefaultValues AnalogInputs::inputsP_[] PROGMEM = {
  {{0, ANALOG_VOLT(0.000)}, {9053, ANALOG_VOLT(4.133)}},              //Vout_plus_pin
  {{0, ANALOG_VOLT(0.000)}, {9053, ANALOG_VOLT(4.133)}},              //Vout_minus_pin
  {{830, ANALOG_AMP(0.050)}, {11350, ANALOG_AMP(1.000)}},             //Ismps
  {{1285, ANALOG_AMP(0.050)}, {8153, ANALOG_AMP(0.300)}},             //Idischarge
  {{0, ANALOG_VOLT(0.000)}, {0, ANALOG_VOLT(0.000)}},                 //VoutMux
  {{0, ANALOG_CELCIUS(0.000)}, {0, ANALOG_CELCIUS(0.000)}},           //Tintern
  {{0, ANALOG_VOLT(0.000)}, {53160, ANALOG_VOLT(15.398)}},            //Vin
  {{5884, 2280}, {0, 0}},                                             //Textern

  {{0, ANALOG_VOLT(0.000)}, {48963, ANALOG_VOLT(3.752)}},             //Vb0_pin
  {{0, ANALOG_VOLT(0.000)}, {54463, ANALOG_VOLT(4.133)}},             //Vb1_pin
  {{0, ANALOG_VOLT(0.000)}, {55727, ANALOG_VOLT(4.200)}},             //Vb2_pin
  {{0, ANALOG_VOLT(0.000)}, {26432, ANALOG_VOLT(2.000)}},             //Vb3_pin
  {{0, ANALOG_VOLT(0.000)}, {54799, ANALOG_VOLT(4.200)}},             //Vb4_pin
  {{0, ANALOG_VOLT(0.000)}, {55496, ANALOG_VOLT(4.200)}},             //Vb5_pin
  {{0, ANALOG_VOLT(0.000)}, {55434, ANALOG_VOLT(4.200)}},             //Vb6_pin

#if MAX_BALANCE_CELLS > 6
  {{,}, {,}},                   //Vb7_pin
  {{,}, {,}},                   //Vb8_pin
#endif
  {{832, ANALOG_AMP(0.050)}, {11351, ANALOG_AMP(1.000)}},             //IsmpsSet
  {{424, ANALOG_AMP(0.050)}, {2972, ANALOG_AMP(0.300)}},              //IdischargeSet
};

namespace
{
  void assert ()
  {
    STATIC_ASSERT (sizeOfArray (AnalogInputs::inputsP_) == AnalogInputs::PHYSICAL_INPUTS);
  }
}
