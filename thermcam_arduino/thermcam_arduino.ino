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
#include <Servo.h>
#include <Wire.h>

static const bool debug = false;
static const bool joy_enabled = true;

#define SERIAL_BAUD_RATE 9600

#define SERVO_X_PIN 9
#define SERVO_X_MIN 30
#define SERVO_X_MAX 170

#define SERVO_Y_PIN 8
#define SERVO_Y_MIN 30
#define SERVO_Y_MAX 165

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

#define SENSOR_SLAVE_ADDRESS 0x5A

enum sensor {ambient = 0x6, object = 0x7};
static bool read_temp(enum sensor s, double *temp);

static int x = 90;
static int y = 90;

static Servo servo_x;
static Servo servo_y;

static char buf[100];

void setup()
{
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println("Isetup");
  sprintf(buf, "Idims:%d,%d,%d,%d", SERVO_X_MIN, SERVO_X_MAX, SERVO_Y_MIN, SERVO_Y_MAX);
  Serial.println(buf);

  servo_x.attach(SERVO_X_PIN);
  servo_y.attach(SERVO_Y_PIN);
  servo_x.write(x);
  servo_y.write(y);
  
  if (joy_enabled)
  {
    pinMode(JOY_BUTTON_PIN, INPUT);
    digitalWrite(JOY_BUTTON_PIN, HIGH);

    int h, v;
    read_joystick(h, v);
    if (h > JOY_HORZ_CENTER + 10 || h < JOY_HORZ_CENTER - 10 || v > JOY_VERT_CENTER + 10 || v < JOY_VERT_CENTER - 10)
      Serial.println("Ejoystick needs recalibration!");
    JOY_HORZ_CENTER = h;
    JOY_VERT_CENTER = v;
  }
  
  Wire.begin();
  
  Serial.println("Isetup finished");
}

static bool read_temp(enum sensor s, double *temp)
{
  int r;
  
  Wire.beginTransmission(SENSOR_SLAVE_ADDRESS);
  Wire.write(s);
  r = Wire.endTransmission(false);
  if (r)
  {
    sprintf(buf, "EendTransmission failed: %d", r);
    Serial.println(buf);
    return false;
  }
   
  unsigned char bytes[3];
  int i = 0;
  Wire.requestFrom(SENSOR_SLAVE_ADDRESS, 3);
  while (Wire.available())
  {
    char c = Wire.read();
    if (i < 3)
      bytes[i] = c;
    i++;
  }
    
  if (bytes[1] & 0x80)
  {
    Serial.println("Eerror bit set");
    return false;
  }

  *temp = ((bytes[1] & 0x7F) << 8) + bytes[0];
  *temp = (*temp * 0.02) - 273.15;
  
  return true;
}

static void move_x(int newpos, bool print_errors = 1)
{
  if (newpos < SERVO_X_MIN)
  {
    if (print_errors)
      Serial.println("Einvalid x pos");
    return;
  }
  if (newpos > SERVO_X_MAX)
  {
    if (print_errors)
      Serial.println("Einvalid x pos");
    return;
  }

  x = newpos;
  servo_x.write(x);
  sprintf(buf, "Ix: %d", x);
  Serial.println(buf);
}

static void move_y(int newpos, bool print_errors = 1)
{
  if (newpos < SERVO_Y_MIN)
  {
    if (print_errors)
      Serial.println("Einvalid y pos");
    return;
  }
  if (newpos > SERVO_Y_MAX)
  {
    if (print_errors)
      Serial.println("Einvalid y pos");
    return;
  }

  y = newpos;
  servo_y.write(y);
  sprintf(buf, "Iy: %d", y);
  Serial.println(buf);
}

static bool joy_suspended = false;

static void read_joystick(int &h, int &v)
{
  h = analogRead(JOY_HORZ_PIN);
  v = analogRead(JOY_VERT_PIN);
}

static void read_joystick(int &h, int &h_scaled, int &v, int &v_scaled, bool &pushed)
{
  read_joystick(h, v);
  pushed = !digitalRead(JOY_BUTTON_PIN);
  h_scaled = 0;
  v_scaled = 0;
 
  if (debug)
  {
    sprintf(buf, "Ijoy prescale: %d %d %d", h, v, pushed);
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

void loop()
{
  char command[100];
  int len = 0, offset = 0;
  if (joy_enabled && !joy_suspended)
  {
    int h, h_scaled, v, v_scaled;
    bool pushed;
    read_joystick(h, h_scaled, v, v_scaled, pushed);
    if (abs(h_scaled) > 10 || abs(v_scaled) > 10 || pushed)
    {
      sprintf(command, "Ijoy: %d %d %d", h_scaled, v_scaled, pushed);
      Serial.println(command);
      if (!pushed)
      {
        if (abs(h_scaled) > 10)
        {
          move_x(x + h_scaled / 10, false);
          // if user wants to set position precisely
          if (abs(h_scaled) < 20)
            delay(100); // wait for joystick to bounce off
        }
        if (abs(v_scaled) > 10)
        {
          move_y(y + v_scaled / 10, false);
          if (abs(v_scaled) < 20)
            delay(100);
        }
      }
    } 
  }
  if (!Serial.available())
    return;
  long start = millis();

  do
  {
    if (len >= 100)
    {
      Serial.println("Etoo long command");
      return;
    }
    
    while (!Serial.available())
    {
      if (millis() > start + 1000)
      {
        Serial.println("Ecommand input timeout");
        return;
      }
    }
    
    command[len] = Serial.read();
  }
  while (command[len++] != '!');
  command[--len] = 0;
  
  if (len < 1)
  {
    Serial.println("Etoo short command");
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
          Serial.println("Einvalid format");
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
        Serial.println("Einvalid p command");
        return;
      }

      if (sscanf(command + 2, "%d", &offset) != 1)
      {
        Serial.println("Einvalid p format");
        return;
      }

      if (command[1] == 'x')
        move_x(offset);
      else if (command[1] == 'y')
        move_y(offset);
      else
        Serial.println("Einvalid p? command");

      break;
    case 't':
    {
      enum sensor s;
      double temp;
      int i;

      if (len < 2)
      {
        Serial.println("Etoo short t command");
        return;
      }

      if (command[1] == 'o')
        s = object;
      else if (command[1] == 'a')
        s = ambient;
      else
      {
        Serial.println("Einvalid sensor");
        return;
      }

//      delay(50);
      for (i = 0; i < 10 && !read_temp(s, &temp); ++i) 
          delay(5000);
      if (i == 10)
        return;

      if (command[1] == 'o')
        Serial.print("Itemp object: ");
      else if (command[1] == 'a')
        Serial.print("Itemp ambient: ");

      Serial.println(temp);
 
      break;
    }
    case 'j':
      if (len < 2)
      {
        Serial.println("Einvalid j command");
        return;
      }
      
      if (command[1] == 'd')
        joy_suspended = true;
      else if (command[1] == 'e')
        joy_suspended = false;
      else
        Serial.println("Einvalid j command");
      
      break;
    default:
      Serial.println("Einvalid command");
  }
}

