#include <Servo.h>
#include <Wire.h>

enum sensor {ambient = 0x6, object = 0x7};
double readTemp(enum sensor s);

Servo servo_x;
Servo servo_y;

int x = 90;
int y = 90;

void setup()
{
  Serial.begin(9600);
  Serial.println("setup");

  servo_x.attach(9);
  servo_y.attach(8);
  servo_x.write(x);
  servo_y.write(y);
  
  Wire.begin();
  
  Serial.println("setup finished");
}

char buf[100];
int f = 5;

double readTemp(enum sensor s)
{
  int r;
  
  Wire.beginTransmission(0x5A);
  Wire.write(s);
  r = Wire.endTransmission(false);
  if (r)
  {
    Serial.println("endTransmission failed");
    return 0;
  }
   
  unsigned char bytes[3];
  int i = 0;
  Wire.requestFrom(0x5A, 3);
  while (Wire.available())
  {
    char c = Wire.read();
    if (i < 3)
      bytes[i] = c;
    i++;
  }
  
  double tempData;
  
  if (bytes[1] & 0x80)
    Serial.println("error!");
  tempData = ((bytes[1] & 0x7F) << 8) + bytes[0];
  tempData = (tempData * 0.02) - 273.15;
  
  return tempData;
}

void loop()
{
  int sp = f, command;
  if (!Serial.available())
    return;

  command = Serial.read();
  if ((command >= '0' && command <= '9') || command == 'x')
  {
    if (command == 'x')
      sp = 180;
    else
      sp = 2 * (command - '0');
    while (!Serial.available())
      ;
    command = Serial.read();
  }

  if (command == 'l')
  {
    x -= sp;
    if (x < 30)
      x = 30;
    servo_x.write(x);
    sprintf(buf, "x: %d", x);
    Serial.println(buf);
  }
  else if (command == 'r')
  {
    x += sp;
    if (x > 170)
      x = 170;
    servo_x.write(x);
    sprintf(buf, "x: %d", x);
    Serial.println(buf);
  }
  else if (command == 'u')
  {
    y += sp;
    if (y > 165)
      y = 165;
    servo_y.write(y);
    sprintf(buf, "y: %d", y);
    Serial.println(buf);
  }
  else if (command == 'd')
  {
    y -= sp;
    if (y < 30)
      y = 30;
    servo_y.write(y);
    sprintf(buf, "y: %d", y);
    Serial.println(buf);
  }
  else if (command == 't')
  {
    Serial.print("object: ");
    Serial.print(readTemp(object));
    Serial.print(", ambient: ");
    Serial.println(readTemp(ambient));
  }
}

