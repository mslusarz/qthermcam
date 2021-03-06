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

//static char tmpbuf[30];
enum ir_button infrared_any_button_pressed()
{
//  if (!irrecv.decode(&results))
  if (!irrecv.decode(NEC, &results))
    return NONE;

  //int type = results.decode_type;
  unsigned long val = results.value;
  irrecv.resume();

  static unsigned long last_value;
  
  if (val == 0xFFFFFFFF)
    val = last_value;

  last_value = val;
  
  //sprintf(tmpbuf, _("Iir %d %lx"), type, val);
  //Serial.println(tmpbuf);

  switch (val)
  {
    case 0x4BB5C03F:
      #if TC_DEBUG > 1
      Serial.println("left");
      #endif
      return LEFT;
    case 0x4BB500FF:
      #if TC_DEBUG > 1
      Serial.println("up");
      #endif
      return UP;
    case 0x4BB540BF:
      #if TC_DEBUG > 1
      Serial.println("right");
      #endif
      return RIGHT;
    case 0x4BB5807F:
      #if TC_DEBUG > 1
      Serial.println("down");
      #endif
      return DOWN;
    case 0x4BB57887:
      #if TC_DEBUG > 1
      Serial.println("left top");
      #endif
      return LEFT_TOP;
    case 0x4BB538C7:
      #if TC_DEBUG > 1
      Serial.println("right bottom");
      #endif
      return RIGHT_BOTTOM;
    case 0x4BB5A05F:
      #if TC_DEBUG > 1
      Serial.println("stop");
      #endif
      return STOP;
    default:
      #if TC_DEBUG > 0
      if (use_serial())
      {
        Serial.print(_("Ei1 "));// unknown IR code
        Serial.println(val, HEX);
      }
      #endif
      break;
  }

  return NONE;
}
#endif

