/////////////////////////////////
// LCD Helpers
// Author: hestenet
// Canonical Repository: https://github.com/hestenet/arduino-shot-timer
///////////////////////////////// 
//   This file is part of ShotTimer. 
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Lesser General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
//    http://www.gnu.org/licenses/lgpl.txt
//
/////////////////////////////////

//////////////
// DEFINITIONS
//////////////

// These #defines make it easy to set the backlight color on the Adafruit RGB LCD
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7


//////////////
// FUNCTIONS
//////////////

/////////////////////////////////////////////////////////////
// PROGMEM Helper - Print a string from PROGMEM to an LCD Screen
/////////////////////////////////////////////////////////////

void lcdPrint_p(Adafruit_RGBLCDShield* lcd, const char * str)
{
  char c;
  if (!str)
  {
    return;
  }
  while (c = pgm_read_byte(str++))
    lcd->print(c);
}

/////////////////////////////////////////////////////////////
// Print to an LCD
/////////////////////////////////////////////////////////////

void lcdPrint(Adafruit_RGBLCDShield* lcd, uint32_t t, byte digits)
{
  DEBUG_PRINT(F("Printing to LCD:"));
  char lcdOutput[11];
  fitDigits(lcdOutput, digits);
  DEBUG_PRINTLN(lcdOutput,0);
  lcd->print(lcdOutput);
}


/////////////////////////////////////////////////////////////
// Print time to an LCD screen
/////////////////////////////////////////////////////////////

void lcdPrintTime(Adafruit_RGBLCDShield* lcd, uint32_t t, byte digits)
{
  DEBUG_PRINTLN(F("Printing Time to LCD"),0);
  char lcdOutput[11];
  convertTime(t, digits, lcdOutput);
  DEBUG_PRINTLN(lcdOutput,0);
  lcd->print(lcdOutput);
}

/////////////////////////////////////////////////////////////
// LCD Helper - 2 digits
/////////////////////////////////////////////////////////////

//void lcd2digits(uint32_t x)
//{
//  if (x < 10) lcd.print(F("0"));
//  //lcd.print(x);
//}

/////////////////////////////////////////////////////////////
// LCD Helper - 3 digits
/////////////////////////////////////////////////////////////
//void lcd3digits(uint32_t x)
//{
//  if (x < 100) lcd.print(F("0"));
//  if (x < 10) lcd.print(F("0"));
//  //lcd.print(x);
//}

/////////////////////////////////////////////////////////////
// LCD Helper - 4 digits
/////////////////////////////////////////////////////////////

//void lcd4digits(uint32_t x) {
//  if (x < 1000) lcd.print(F("0"));
//  if (x < 100) lcd.print(F("0"));
//  if (x < 10) lcd.print(F("0"));
//  //lcd.print(x);
//}
