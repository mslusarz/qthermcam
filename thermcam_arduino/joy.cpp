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
#include "joy.h"

#if JOY_ENABLED == 1
static void read_joystick(int &h, int &v);

#define JOY_BUTTON_PIN 4
#define JOY_HORZ_PIN 2
#define JOY_VERT_PIN 3

#define JOY_HORZ_LEFT   0
#define JOY_HORZ_DEFAULT_CENTER 491
static int JOY_HORZ_CENTER = JOY_HORZ_DEFAULT_CENTER;
#define JOY_HORZ_RIGHT  1008

#define JOY_VERT_UP     95
#define JOY_VERT_DEFAULT_CENTER 547
static int JOY_VERT_CENTER = JOY_VERT_DEFAULT_CENTER;
#define JOY_VERT_DOWN   1023

void joy_init()
{
  pinMode(JOY_BUTTON_PIN, INPUT);
  digitalWrite(JOY_BUTTON_PIN, HIGH);

  int h, v;
  read_joystick(h, v);
  if (h > JOY_HORZ_CENTER + 10 || h < JOY_HORZ_CENTER - 10 || v > JOY_VERT_CENTER + 10 || v < JOY_VERT_CENTER - 10)
    Serial.println("Ej1"); // joystick needs recalibration!
  JOY_HORZ_CENTER = h;
  JOY_VERT_CENTER = v;
}

static void read_joystick(int &h, int &v)
{
  h = analogRead(JOY_HORZ_PIN);
  v = analogRead(JOY_VERT_PIN);
}

bool joystick_button_pressed()
{
  return !digitalRead(JOY_BUTTON_PIN);
}

void read_joystick(int &h, int &h_scaled, int &v, int &v_scaled, bool &pressed)
{
  read_joystick(h, v);
  pressed = joystick_button_pressed();
  h_scaled = 0;
  v_scaled = 0;
 
  if (TC_DEBUG >= 2)
  {
    sprintf(buf, "Ijp: %d %d %d", h, v, pressed); // joy prescale
    Serial.println(buf);
  }

  if (h < JOY_HORZ_CENTER)
    h_scaled = max(-100, map(h, JOY_HORZ_LEFT,   JOY_HORZ_CENTER, -100, 0));
  else if (h > JOY_HORZ_CENTER)
    h_scaled = min(100,  map(h, JOY_HORZ_CENTER, JOY_HORZ_RIGHT,     0, 100));
  if (v < JOY_VERT_CENTER)
    v_scaled = min(100,  map(v, JOY_VERT_UP,     JOY_VERT_CENTER, 100, 0));
  else if (v > JOY_VERT_CENTER)
    v_scaled = max(-100, map(v, JOY_VERT_CENTER, JOY_VERT_DOWN,      0, -100));
}

#endif

