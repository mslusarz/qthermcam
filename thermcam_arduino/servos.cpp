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
#include "servos.h"
#include "common.h"
#include "Arduino.h"
#include "Servo.h"
#include <stdio.h>

#define SERVO_X_PIN 9
#define SERVO_X_MIN 30
#define SERVO_X_MAX 170

#define SERVO_Y_PIN 8
#define SERVO_Y_MIN 30
#define SERVO_Y_MAX 165

static Servo servo_x;
static Servo servo_y;

int x = 90;
int y = 90;

void servo_init()
{
  sprintf(buf, "Idims:%d,%d,%d,%d", SERVO_X_MIN, SERVO_X_MAX, SERVO_Y_MIN, SERVO_Y_MAX);
  Serial.println(buf);

  servo_x.attach(SERVO_X_PIN);
  servo_y.attach(SERVO_Y_PIN);
  servo_x.write(x);
  servo_y.write(y);
}

bool move_x(int newpos, bool print_errors)
{
  if (newpos < SERVO_X_MIN)
  {
    if (x == SERVO_X_MIN)
    {
      if (print_errors)
        println("Einvalid x pos");
      return false;
    }
    newpos = SERVO_X_MIN;
  }
  
  if (newpos > SERVO_X_MAX)
  {
    if (x == SERVO_X_MAX)
    {
      if (print_errors)
        println("Einvalid x pos");
      return false;
    }
    newpos = SERVO_X_MAX;
  }

  x = newpos;
  servo_x.write(x);
  sprintf(buf, "Ix: %d", x);
  println(buf);
  return true;
}

bool move_y(int newpos, bool print_errors)
{
  if (newpos < SERVO_Y_MIN)
  {
    if (y == SERVO_Y_MIN)
    {
      if (print_errors)
        println("Einvalid y pos");
      return false;
    }
    newpos = SERVO_Y_MIN;
  }
  
  if (newpos > SERVO_Y_MAX)
  {
    if (y == SERVO_Y_MAX)
    {
      if (print_errors)
        println("Einvalid y pos");
      return false;
    }
    newpos = SERVO_Y_MAX;
  }

  y = newpos;
  servo_y.write(y);
  sprintf(buf, "Iy: %d", y);
  println(buf);
  return true;
}

/* Servos need to be refreshed every 20ms, so servo library uses hardware timer (timer1 on
   Uno) to do this. If this timer interrupt will be delayed because of some other lengthy
   interrupt (like I2C communication), library will miss deadline and servo will move
   randomly (20us delay is enough to trigger it). So if we want to run our interrupt-using
   code and do not mess up servos, we need to wait for big enough window, by watching when
   timer interrupt is supposed to trigger and delaying our code after it.
   Yes, it sucks.
 */
void servos_alloc_time(int us)
{
  int step = us / 10;
  if (step == 0)
    step = 1;
#if defined(_useTimer1) && !defined(_useTimer2) && !defined(_useTimer3) && !defined(_useTimer4) && !defined(_useTimer5)
  while (abs(OCR1A - TCNT1) < us * 2) // where is the "2" factor coming from?
#else
  #error unhandled configuration
#endif
    delayMicroseconds(step);
}

