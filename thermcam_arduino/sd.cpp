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
#include "sd.h"
#ifdef SD_ENABLED
#include "common.h"

#include <SPI.h>
#include <SD.h>

#define SD_CS_PIN 10
#define SD_POWER_ENABLE_PIN 3
#define SD_INPUT_DISABLE_PIN 7

static File file;
static bool sd_ok;
static char file_name[13];

void sd_init()
{
  pinMode(SD_CS_PIN, OUTPUT);
  pinMode(SD_POWER_ENABLE_PIN, OUTPUT);
  pinMode(SD_INPUT_DISABLE_PIN, OUTPUT);

  digitalWrite(SD_POWER_ENABLE_PIN, LOW);   // turns mosfet off
  digitalWrite(SD_INPUT_DISABLE_PIN, HIGH); // disables sck, mosi, cs pins (OE pins of 74HC125 buffer)

  delay(100);

  digitalWrite(SD_POWER_ENABLE_PIN, HIGH);  // turns mosfet on
  digitalWrite(SD_INPUT_DISABLE_PIN, LOW);  // enables sck, mosi, cs pins (OE pins of 74HC125 buffer)
  
  sd_ok = SD.begin(SD_CS_PIN);
  if (!sd_ok)
    Serial.println("Ed1"); // sd initialization failed!
}

static int ymin_, ymax_, xmin_, xmax_;

void sd_open_new_file(int ymin, int ymax, int xmin, int xmax)
{
  if (!sd_ok)
    return;

  strcpy(file_name, "test.qtc");
  file = SD.open(file_name, FILE_WRITE);
  if (!file)
  {
    Serial.println("Ed3");
    return;
  }

  sprintf(buf, "ymin=\"%d\" xmin=\"%d\" ymax=\"%d\" xmax=\"%d\"", ymin, xmin, ymax, xmax);
  file.print("<!DOCTYPE qtdc>\n<qtcd>\n <fov ");
  file.print(buf);
  file.print("/>\n <data>\n");
  ymin_ = ymin;
  ymax_ = ymax;
  xmin_ = xmin;
  xmax_ = xmax;
}

void sd_dump_data(double *temps, int y, int x_start, int x_count)
{
  if (!sd_ok || !file)
    return;

  if (x_start == xmin_)
  {
    sprintf(buf, "  <row y=\"%d\">\n", y);
    file.print(buf);
  }

  for (int i = 0; i < x_count; ++i)
  {
    sprintf(buf, "   <col x=\"%d\" val=\"", x_start + i);
    file.print(buf);
    file.print(temps[i]);
    file.print("\"/>\n");
  }

  if (x_start + x_count - 1 == xmax_)
    file.print("  </row>\n");
}

void sd_remove_file()
{
  if (!sd_ok || !file)
    return;

  file.close();

  if (!SD.remove(file_name))
    Serial.println("Ed2"); // couldn't remove file
}

void sd_close_file()
{
  if (!sd_ok || !file)
    return;

  file.print(" </data>\n</qtdc>\n");
  file.close();
}
#endif

