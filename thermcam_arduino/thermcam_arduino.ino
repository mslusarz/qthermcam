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

#include <SPI.h>
#include <SD.h>
#include <stdlib.h>

#include "common.h"
#include "ir.h"
#include "joy.h"
#include "sd.h"
#include "servos.h"
#include "temp.h"

static bool joy_suspended = false;

#define SERIAL_BAUD_RATE 115200

static enum {AUTO, MANUAL} mode = AUTO;

static char _buf[100];
const char *pgm2ram(PROGMEM char *str)
{
  strcpy_P(_buf, str);
  return _buf;
}

bool use_serial()
{
  return mode == MANUAL || TC_DEBUG;
}

void print(const char *s)
{
  if (use_serial())
    Serial.print(s);
}

void print(char c)
{
  if (use_serial())
    Serial.print(c);
}

void print(double f)
{
  if (use_serial())
    Serial.print(f);
}

void print(int i)
{
  if (use_serial())
    Serial.print(i);
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

void println(int i)
{
  if (use_serial())
    Serial.println(i);
}

#define LASER_ENABLE_PIN 5

void setup()
{
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println(_("Is")); // setup

  pinMode(LASER_ENABLE_PIN, OUTPUT);
  digitalWrite(LASER_ENABLE_PIN, HIGH);

  servo_init();
  joy_init();
  ir_init();
  temp_init();
  sd_init();
  
  Serial.println(_("Isf")); // setup finished
}

#define MAX_TEMPS 10
double temps[MAX_TEMPS];

static void scan(int left, int top, int right, int bottom)
{
  bool aborted = false;
  int tmp, temp_count;
  print(_("Isc ")); // scanning
  print(left);
  print(_(", "));
  print(top);
  print(_(" -> "));
  print(right);
  print(_(", "));
  println(bottom);

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
  sd_open_new_file(bottom, top, left, right);

  for (int i = top; i >= bottom && !aborted; i--)
  {
    int k, t;
    move_y(i);
    move_x(left);
    
    temp_count = 0;
    
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
        temps[temp_count++] = temp;
        if (temp_count == MAX_TEMPS || j == right)
        {
          sd_dump_data(temps, i, j - temp_count + 1, temp_count);
          temp_count = 0;
        }
        print(_("IA "));
        print(x);
        print(' ');
        print(y);
        print(' ');

        println(temp);
        aborted = joystick_button_pressed() || infrared_stop_button_pressed();
      }
    }
  }

  if (aborted)
  {
    sd_remove_file();
    while (joystick_button_pressed())
      delay(50);
    delay(100);
  }
  else
    sd_close_file();

  println(_("Isc f")); // scanning finished
}

#define MAX_COMMAND_LENGTH 50
static char command[MAX_COMMAND_LENGTH];

void loop()
{
  int len = 0, offset = 0;

  if (JOY_ENABLED && !joy_suspended)
  {
    int h, h_scaled, v, v_scaled;
    bool pressed;
    
    read_joystick(h, h_scaled, v, v_scaled, pressed);
    
    if (abs(h_scaled) > 10 || abs(v_scaled) > 10 || pressed)
    {
      #if TC_DEBUG > 0
      print(_("Ijoy: "));
      print(h_scaled);
      print(' ');
      print(v_scaled);
      print(' ');
      println(pressed);
      #endif
  
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
            println(_("E00")); // joystick button is disabled in manual mode
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
        println(_("E01")); // infrared start button is disabled in manual mode
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
    if (len >= MAX_COMMAND_LENGTH)
    {
      println(_("E02")); // too long command
      return;
    }
    
    while (!Serial.available())
    {
      if (millis() > start + 1000)
      {
        println(_("E03")); // command input timeout
        return;
      }
    }
    
    command[len] = Serial.read();
  }
  while (command[len++] != '!');
  command[--len] = 0;
  
  if (len < 1)
  {
    println(_("E04")); // too short command
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
          println(_("E05")); // invalid format
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
        println(_("E06")); // invalid p command
        return;
      }

      if (sscanf(command + 2, "%d", &offset) != 1)
      {
        println(_("E07")); // invalid p format
        return;
      }

      if (command[1] == 'x')
        move_x(offset);
      else if (command[1] == 'y')
        move_y(offset);
      else
        println(_("E08")); // invalid p? command

      break;
    case 't':
    {
      enum sensor s;
      double temp;
      int i;

      if (len < 2)
      {
        println(_("E09")); // too short t command
        return;
      }

      if (command[1] == 'o')
        s = object;
      else if (command[1] == 'a')
        s = ambient;
      else
      {
        println(_("E10")); // invalid sensor
        return;
      }

//      delay(50);
      for (i = 0; i < 10 && !read_temp(s, &temp); ++i) 
          delay(5000);
      if (i == 10)
        return;

      if (command[1] == 'o')
        print(_("Ito: ")); // temp object
      else if (command[1] == 'a')
        print(_("Ita: ")); // temp ambient

      println(temp);
 
      break;
    }
    case 'j':
      if (len < 2)
      {
        println(_("E11")); // invalid j command
        return;
      }
      
      if (command[1] == 'd')
        joy_suspended = true;
      else if (command[1] == 'e')
        joy_suspended = false;
      else
        println(_("E12")); // invalid j command
      
      break;
    case 'm':
      if (strcmp(command, _("mon")) == 0) // "manual on"
        mode = MANUAL;
      else if (strcmp(command, _("moff")) == 0) // "manual off"
        mode = AUTO;
      else
        println(_("E13")); // invalid m command

      break;
    default:
      println(_("E14")); // invalid command
  }
}

