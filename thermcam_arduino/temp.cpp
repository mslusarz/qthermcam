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
#include "servos.h"
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
  servos_alloc_time(300);
  r = Wire.endTransmission(false);
  if (r)
  {
    print("Et1 "); // endTransmission failed
    println(r);
    return false;
  }
   
  unsigned char bytes[3];
  int i = 0;
  servos_alloc_time(550);
  int ret = Wire.requestFrom(SENSOR_SLAVE_ADDRESS, 3);
  if (ret != 3)
  {
    print("Et2 "); // requestFrom returned 
    println(ret);
    return false;
  }

  while (Wire.available())
  {
    char c = Wire.read();
    if (i < 3)
      bytes[i] = c;
    i++;
  }
    
  if (bytes[1] & 0x80)
  {
    println("Et3"); // error bit set
    return false;
  }

  *temp = ((bytes[1] & 0x7F) << 8) + bytes[0];
  *temp = (*temp * 0.02) - 273.15;
  
  return true;
}

