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

