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

// Arduino's buggy build system - if I don't include these headers here, they won't be available to other files
#include <Servo.h>
#include <Wire.h>
#include <IRremote.h>

#include "common.h"
#include "ir.h"
#include "joy.h"
#include "servos.h"
#include "temp.h"

const int debug = 0;
static const bool joy_enabled = true;
static bool joy_suspended = false;

#define SERIAL_BAUD_RATE 115200

static enum {AUTO, MANUAL} mode = AUTO;

char buf[100];

bool use_serial()
{
  return mode == MANUAL || debug;
}

void print(const char *s)
{
  if (use_serial())
    Serial.print(s);
}

void print(double f)
{
  if (use_serial())
    Serial.print(f);
}

void println(const char *s)
{
  if (use_serial())
    Serial.println(s);
}

void println(double f)
{
  if (use_serial())
    Serial.println(f);
}

void setup()
{
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println("Isetup");

  servo_init();
  if (joy_enabled)
    joy_init();
  ir_init();
  
  temp_init();
  
  Serial.println("Isetup finished");
}


static void scan(int left, int top, int right, int bottom)
{
  bool aborted = false;
  int tmp;
  sprintf(buf, "Iscanning %d, %d -> %d, %d", left, top, right, bottom);
  println(buf);

  if (top < bottom)
  {
    tmp = top;
    top = bottom;
    bottom = tmp;
  }

  if (left > right)
  {
    tmp = left;
    left = right;
    right = tmp;
  }

  for (int i = top; i >= bottom && !aborted; i--)
  {
    int k, t;
    move_y(i);
    move_x(left);
    
    // wait for servos
    for (k = 0; k < 3 && !aborted; k++)
    {
      delay(100);
      aborted = joystick_button_pressed() || infrared_stop_button_pressed();
    }

    for (int j = left; j <= right && !aborted; j++)
    {
      move_x(j);

      // servo and sensor stabilisation
      delay(100);

      double temp;
      for (t = 0; t < 10 && !read_temp(object, &temp) && !aborted; ++t)
      {
        // wait 5000ms
        for (k = 0; k < 50 && !aborted; k++)
        {
          delay(100);
          aborted = joystick_button_pressed() || infrared_stop_button_pressed();
        }
      }

      if (t == 10) // sensor is broken/disconnected
        aborted = true; // don't bother reading more data
      else
      {
        sprintf(buf, "IA %d %d ", x, y);
        print(buf);
        println(temp);
        aborted = joystick_button_pressed() || infrared_stop_button_pressed();
      }
    }
  }

  if (aborted)
  {
    while (joystick_button_pressed())
      delay(50);
    delay(100);
  }
  println("Iscanning finished");
}

void loop()
{
  char command[100];
  int len = 0, offset = 0;

  if (joy_enabled && !joy_suspended)
  {
    int h, h_scaled, v, v_scaled;
    bool pressed;
    
    read_joystick(h, h_scaled, v, v_scaled, pressed);
    
    if (abs(h_scaled) > 10 || abs(v_scaled) > 10 || pressed)
    {
      sprintf(command, "Ijoy: %d %d %d", h_scaled, v_scaled, pressed);
      println(command);
  
      if (!pressed)
      {
        int sleep = 0;

        if (abs(h_scaled) > 10)
        {
          if (move_x(x + h_scaled / 10, false))
          {
            // if user wants to set position precisely
            if (abs(h_scaled) < 20)
              sleep = 100; // wait for joystick to bounce off
            else
              sleep = 20;
          }
        }
  
        if (abs(v_scaled) > 10)
        {
          if (move_y(y + v_scaled / 10, false))
          {
            if (abs(v_scaled) < 20)
              sleep = 100;
            else
              sleep = sleep > 20 ? sleep : 20;
          }
        }

        if (sleep)
          delay(sleep);
      }
      else
      {
        static unsigned long lastButtonTime = 0;
        
        if (mode == MANUAL)
        {
          if (millis() - lastButtonTime > 1000)
            println("Ejoystick button is disabled in manual mode");
        }
        else
        {
          static int left = 0, top;
          
          while (joystick_button_pressed())
            delay(50);
          delay(100);

          if (left == 0)
          {
            left = x;
            top = y;
          }
          else
          {
            scan(left, top, x, y);
            left = 0;
          }
        }
        
        lastButtonTime = millis();
      }
    } 
  }

  enum ir_button button = infrared_any_button_pressed();
  static int left = 0, top;
  switch (button)
  {
    case LEFT:
      move_x(x - 10, false);
      break;
    case UP:
      move_y(y + 10, false);
      break;
    case RIGHT:
      move_x(x + 10, false);
      break;
    case DOWN:
      move_y(y - 10, false);
      break;
    case LEFT_TOP:
      left = x;
      top = y;
      break;
    case RIGHT_BOTTOM:
      if (mode == MANUAL)
        println("Einfrared start button is disabled in manual mode");
      else
        scan(left, top, x, y);
      break;
    default:
      break;
  }

  if (!Serial.available())
    return;
  
  long start = millis();

  do
  {
    if (len >= 100)
    {
      println("Etoo long command");
      return;
    }
    
    while (!Serial.available())
    {
      if (millis() > start + 1000)
      {
        println("Ecommand input timeout");
        return;
      }
    }
    
    command[len] = Serial.read();
  }
  while (command[len++] != '!');
  command[--len] = 0;
  
  if (len < 1)
  {
    println("Etoo short command");
    return;
  }

  switch (command[0])
  {
    case 'l':
    case 'r':
    case 'u':
    case 'd':
      if (len == 1)
        offset = 1;
      else
        if (sscanf(command + 1, "%d", &offset) != 1)
        {
          println("Einvalid format");
          return;
        }

      if (command[0] == 'l')
        move_x(x - offset);
      else if (command[0] == 'r')
        move_x(x + offset);
      else if (command[0] == 'd')
        move_y(y - offset);
      else if (command[0] == 'u')
        move_y(y + offset);

      break;
    case 'p':
      if (len < 3)
      {
        println("Einvalid p command");
        return;
      }

      if (sscanf(command + 2, "%d", &offset) != 1)
      {
        println("Einvalid p format");
        return;
      }

      if (command[1] == 'x')
        move_x(offset);
      else if (command[1] == 'y')
        move_y(offset);
      else
        println("Einvalid p? command");

      break;
    case 't':
    {
      enum sensor s;
      double temp;
      int i;

      if (len < 2)
      {
        println("Etoo short t command");
        return;
      }

      if (command[1] == 'o')
        s = object;
      else if (command[1] == 'a')
        s = ambient;
      else
      {
        println("Einvalid sensor");
        return;
      }

//      delay(50);
      for (i = 0; i < 10 && !read_temp(s, &temp); ++i) 
          delay(5000);
      if (i == 10)
        return;

      if (command[1] == 'o')
        print("Itemp object: ");
      else if (command[1] == 'a')
        print("Itemp ambient: ");

      println(temp);
 
      break;
    }
    case 'j':
      if (len < 2)
      {
        println("Einvalid j command");
        return;
      }
      
      if (command[1] == 'd')
        joy_suspended = true;
      else if (command[1] == 'e')
        joy_suspended = false;
      else
        println("Einvalid j command");
      
      break;
    case 'm':
      if (strcmp(command, "mon") == 0) // "manual on"
        mode = MANUAL;
      else if (strcmp(command, "moff") == 0) // "manual off"
        mode = AUTO;
      else
        println("Einvalid m command");

      break;
    default:
      println("Einvalid command");
  }
}

