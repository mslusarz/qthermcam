/*
    Copyright 2013 Marcin Slusarz <marcin.slusarz@gmail.com>

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

#include "Arduino.h"
#include "ir.h"
#include <IRremote.h>

#if IR_ENABLED == 1

#define IR_RECV_PIN 6
static IRrecv irrecv(IR_RECV_PIN);
static decode_results results;

void ir_init()
{
  irrecv.enableIRIn();
}

bool infrared_stop_button_pressed()
{
  // https://github.com/mslusarz/Arduino-IRremote.git
  bool b = irrecv.decode(NEC, &results);
  if (b)
  {
    b = results.value == 0x4BB5A05F;
    irrecv.resume();
  }
  return b;
}

enum ir_button infrared_any_button_pressed()
{
  if (!irrecv.decode(NEC, &results))
    return NONE;

  unsigned long val = results.value;
  irrecv.resume();

  static unsigned long last_value;
  
  if (val == 0xFFFFFFFF)
    val = last_value;

  last_value = val;

  switch (val)
  {
    case 0x4BB5C03F:
      return LEFT;
    case 0x4BB500FF:
      return UP;
    case 0x4BB540BF:
      return RIGHT;
    case 0x4BB5807F:
      return DOWN;
    case 0x4BB57887:
      return LEFT_TOP;
    case 0x4BB538C7:
      return RIGHT_BOTTOM;
    case 0x4BB5A05F:
      return STOP;
    default:
      #if TC_DEBUG > 0
      if (use_serial())
      {
        Serial.println(_("Ei1 ")); // unknown IR code
        Serial.println(val, HEX);
      }
      #endif
      break;
  }

  return NONE;
}
#endif

