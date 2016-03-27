////////////////////////////////////////////////////////////
// LCD Helpers
// Author: hestenet
// Canonical Repository: https://github.com/hestenet/arduino-shot-timer
//////////////////////////////////////////////////////////// 
//  This file is part of ShotTimer. 
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//  http://www.gnu.org/licenses/lgpl.txt
//
////////////////////////////////////////////////////////////
//
// @TODO: Consider compatibility with MicroLCD for the 0.96" IC2 OLED
// http://forum.arduino.cc/index.php?topic=105866.30
//
// @TODO: Figure out the processing/memory cost of the time display conversion
//
////////////////////////////////////////////////////////////

#ifndef LCDHELPERS_H_
#define LCDHELPERS_H_

  ////////////////////////////////////////////////////////////
  // DEFINITIONS
  ////////////////////////////////////////////////////////////
  
  // These #defines make it easy to set the backlight color on the 
  // Adafruit RGB LCD
  #define RED 0x1
  #define YELLOW 0x3
  #define GREEN 0x2
  #define TEAL 0x6
  #define BLUE 0x4
  #define VIOLET 0x5
  #define WHITE 0x7
  
  ////////////////////////////////////////////////////////////
  // FUNCTIONS
  ////////////////////////////////////////////////////////////
  
  //////////////////////////////
  // PROGMEM Helper - Print a string from PROGMEM to an LCD Screen
  //////////////////////////////
  
  void lcd_print_p(Adafruit_RGBLCDShield* lcd, const char * kStr)
  {
    char c;
    if (!kStr)
    {
      return;
    }
    while (c = pgm_read_byte(kStr++))
      lcd->print(c);
  }
  
  //////////////////////////////
  // Print to an LCD
  //////////////////////////////
  
  void lcd_print(Adafruit_RGBLCDShield* lcd, uint32_t t, byte digits)
  {
    DEBUG_PRINT(F("Printing to LCD:"));
    char lcd_output[11];
    convert_num(t, digits, lcd_output);
    DEBUG_PRINTLN(lcd_output,0);
    lcd->print(lcd_output);
  }
  
  //////////////////////////////
  // Print time to an LCD screen
  //////////////////////////////
  
  void lcd_print_time(Adafruit_RGBLCDShield* lcd, uint32_t t, byte digits)
  {
    DEBUG_PRINTLN(F("Printing Time to LCD"),0);
    char lcd_output[11];
    convert_time(t, digits, lcd_output);
    DEBUG_PRINTLN(lcd_output,0);
    lcd->print(lcd_output);
  }
  
#endif // LCDHELPERS_H_