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
#include <IRremote.h>

static const int debug = 0;
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

static enum {AUTO, MANUAL} mode = AUTO;

static int x = 90;
static int y = 90;

static Servo servo_x;
static Servo servo_y;

#define IR_RECV_PIN 6
static IRrecv irrecv(IR_RECV_PIN);
static decode_results results;

static char buf[100];

static inline void print(const char *s)
{
  if (mode == MANUAL || debug)
    Serial.print(s);
}

static inline void print(double f)
{
  if (mode == MANUAL || debug)
    Serial.print(f);
}

static inline void println(const char *s)
{
  if (mode == MANUAL || debug)
    Serial.println(s);
}

static inline void println(double f)
{
  if (mode == MANUAL || debug)
    Serial.println(f);
}

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
  irrecv.enableIRIn();
  
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
    println(buf);
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
    println("Eerror bit set");
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
      println("Einvalid x pos");
    return;
  }
  if (newpos > SERVO_X_MAX)
  {
    if (print_errors)
      println("Einvalid x pos");
    return;
  }

  x = newpos;
  servo_x.write(x);
  sprintf(buf, "Ix: %d", x);
  println(buf);
}

static void move_y(int newpos, bool print_errors = 1)
{
  if (newpos < SERVO_Y_MIN)
  {
    if (print_errors)
      println("Einvalid y pos");
    return;
  }
  if (newpos > SERVO_Y_MAX)
  {
    if (print_errors)
      println("Einvalid y pos");
    return;
  }

  y = newpos;
  servo_y.write(y);
  sprintf(buf, "Iy: %d", y);
  println(buf);
}

static bool joy_suspended = false;

static void read_joystick(int &h, int &v)
{
  h = analogRead(JOY_HORZ_PIN);
  v = analogRead(JOY_VERT_PIN);
}

static bool joystick_button_pressed()
{
  return !digitalRead(JOY_BUTTON_PIN);
}

static bool infrared_stop_button_pressed()
{
  bool b = irrecv.decode(&results);
  if (b)
  {
    b = results.decode_type == NEC && results.value == 0x4BB5A05F;
    irrecv.resume();
  }
  return b;
}

static void read_joystick(int &h, int &h_scaled, int &v, int &v_scaled, bool &pressed)
{
  read_joystick(h, v);
  pressed = joystick_button_pressed();
  h_scaled = 0;
  v_scaled = 0;
 
  if (debug >= 2)
  {
    sprintf(buf, "Ijoy prescale: %d %d %d", h, v, pressed);
    println(buf);
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
        if (abs(h_scaled) > 10)
        {
          move_x(x + h_scaled / 10, false);
          // if user wants to set position precisely
          if (abs(h_scaled) < 20)
            delay(100); // wait for joystick to bounce off
          else
            delay(10);
        }
  
        if (abs(v_scaled) > 10)
        {
          move_y(y + v_scaled / 10, false);
          if (abs(v_scaled) < 20)
            delay(100);
          else
            delay(10);
        }
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

  if (irrecv.decode(&results))
  {
    int type = results.decode_type;
    unsigned long val = results.value;
    irrecv.resume();

    if (type == NEC)
    {
      static unsigned long last_value;
      static int left = 0, top;
      
      if (val == 0xFFFFFFFF)
        val = last_value;

      switch (val)
      {
        case 0x4BB5C03F:
          move_x(x - 10, false);
          break;
        case 0x4BB500FF:
          move_y(y + 10, false);
          break;
        case 0x4BB540BF:
          move_x(x + 10, false);
          break;
        case 0x4BB5807F:
          move_y(y - 10, false);
          break;
        case 0x4BB57887:
          left = x;
          top = y;
          break;
        case 0x4BB538C7:
          if (mode == MANUAL)
            println("Einfrared start button is disabled in manual mode");
          else
            scan(left, top, x, y);
          break;
        default:
          println("Unknown IR code ");
          if (mode == MANUAL || debug)
            Serial.println(val, HEX);
          break;
      }

      last_value = val;
    }
    
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

