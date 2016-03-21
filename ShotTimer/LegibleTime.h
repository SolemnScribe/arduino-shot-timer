/////////////////////////////////
// Making Time Legible
// Author: hestenet
// Canonical Repository: https://github.com/hestenet/arduino-shot-timer
//
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

/////////////////////////////////////////////////////////////
// Serial Helper - Print time to a serial monitor
/////////////////////////////////////////////////////////////
//Consider making digits into an enum
//Possibly add another function or another parameter to return strings of a certain length
//String management reference:https://hackingmajenkoblog.wordpress.com/2016/02/04/the-evils-of-arduino-strings/

//serialPrintln
void convertTime(uint32_t te, byte digits, char* str) //formerly called print2digits: http://arduino.cc/forum/index.php?topic=64024.30
{
  //byte h, m, s // Can we store these and do UL math on them?

  strcpy(str,"00:00:00.000"); //pre-set -- should this go in PROGMEM? Experiment later - A: Probably not - I'm going to manipulate it quite a bit every time this is called. It will wind up in memory as STR anyway

  uint32_t tx; // Used to store the portion of time elapsed that matches the resolution(hours,mins,seconds) I am calculating
  uint32_t hdiv = 3600000;  //hours divisor
  uint32_t mdiv = 60000;  //minutes divisor
  uint32_t sdiv = 1000;  //seconds divisor

  // HOURS
  tx = te / hdiv;
  // The following prints the digits in order of ascending significance (i.e. units, then tens, etc.) up to a maximum of the number of digits I care to keep:
  // Based on this example: http://stackoverflow.com/questions/1397737/how-to-get-the-digits-of-a-number-without-converting-it-to-a-string-char-array
  // Make this a helper function?
  for (char i = 1; i >= 0; i--) //i starts at the array index of the smallest digit in the substring, and ends of at the array index of the largest digit in the substring
  {
    int digit = tx % 10;
    str[i] = '0' + digit;
    tx /= 10;
  }
  te %= hdiv; // or if modulo does not work: tx -= x * hdiv;
  DEBUG_PRINTLN(str, 0);

  // MINUTES
  tx = te / mdiv;
  for (char i = 4; i >= 3; i--) //i starts at the array index of the smallest digit in the substring, and ends of at the array index of the largest digit in the substring
  {
    int digit = tx % 10;
    str[i] = '0' + digit;
    tx /= 10;
  }
  te %= mdiv;
  DEBUG_PRINTLN(str, 0);

  // SECONDS
  tx = te / sdiv;
  for (char i = 7; i >= 6; i--) //i starts at the array index of the smallest digit in the substring, and ends of at the array index of the largest digit in the substring
  {
    int digit = tx % 10;
    str[i] = '0' + digit;
    tx /= 10;
  }
  te %= sdiv;
  DEBUG_PRINTLN(str, 0);

  //MILLISECONDS
  tx = te;
  for (char i = 11; i >= 9; i--) //i starts at the array index of the smallest digit in the substring, and ends of at the array index of the largest digit in the substring
  {
    int digit = tx % 10;
    str[i] = '0' + digit;
    tx /= 10;
  }
  DEBUG_PRINTLN(str, 0);
}
