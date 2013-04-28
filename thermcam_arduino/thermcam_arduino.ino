#include <Servo.h>
#include <Wire.h>

#define SERIAL_BAUD_RATE 9600

#define SERVO_X_PIN 9
#define SERVO_X_MIN 30
#define SERVO_X_MAX 170

#define SERVO_Y_PIN 8
#define SERVO_Y_MIN 30
#define SERVO_Y_MAX 165

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
    Serial.println("EendTransmission failed");
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

static void move_x(int newpos)
{
  if (newpos < SERVO_X_MIN)
  {
    Serial.println("Einvalid x pos");
    return;
  }
  if (newpos > SERVO_X_MAX)
  {
    Serial.println("Einvalid x pos");
    return;
  }

  x = newpos;
  servo_x.write(x);
  sprintf(buf, "Ix: %d", x);
  Serial.println(buf);
}

static void move_y(int newpos)
{
  if (newpos < SERVO_Y_MIN)
  {
    Serial.println("Einvalid y pos");
    return;
  }
  if (newpos > SERVO_Y_MAX)
  {
    Serial.println("Einvalid y pos");
    return;
  }

  y = newpos;
  servo_y.write(y);
  sprintf(buf, "Iy: %d", y);
  Serial.println(buf);
}

void loop()
{
  char command[100];
  int len = 0, offset = 0;
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
      if (!read_temp(s, &temp))
        return;

      if (command[1] == 'o')
        Serial.print("Itemp object: ");
      else if (command[1] == 'a')
        Serial.print("Itemp ambient: ");

      Serial.println(temp);
 
      break;
    }
    default:
      Serial.println("Einvalid command");
  }
}

