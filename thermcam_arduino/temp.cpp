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

#include "temp.h"
#include "common.h"
#include <Wire.h>

#define SENSOR_SLAVE_ADDRESS 0x5A

void temp_init()
{
  Wire.begin();
}

bool read_temp(enum sensor s, double *temp)
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

