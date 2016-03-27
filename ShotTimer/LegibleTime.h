////////////////////////////////////////////////////////////
// Making Time Legible
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

#ifndef LEGIBLETIME_H_
#define LEGIBLETIME_H_

  //////////////////////////////
  // Fit Digits - trim an array to the specified number of digits
  //////////////////////////////
  // Function to trim the array to the number of digits specified
  //http://stackoverflow.com/questions/12337836/shifting-elements-in-an-array-in-c-pointer-based
  // Alternate method to count digits: http://stackoverflow.com/questions/1489830/efficient-way-to-determine-number-of-digits-in-an-integer <-- COME BACK TO THIS ONE!
  // http://stackoverflow.com/questions/12692221/deleting-n-first-chars-from-c-string
  // http://forum.arduino.cc/index.php?topic=87303.0
  // http://forum.arduino.cc/index.php?topic=207965.0
  // http://stackoverflow.com/questions/1726298/strip-first-and-last-character-from-c-string
  //////////////////////////////

  // num of digits must include punctuation - format "00:00:00.000"
  void fit_digits(char *str, int digits) 
  {
    size_t len = strlen(str);
    //assert(len >= 2); // or whatever you want to do with short strings
    memmove(str, str+(len-digits), len-1);
    str[len-1] = 0;
    //int newIndex = strlen(str) - digits;
    //for (int i = 0; i != newIndex - 1; i++) 
    //{
    //  *(str+i) = *(str+i+2);
    //}
  }
  
  //////////////////////////////
  // Convert Num - to specific digits
  //////////////////////////////
  // formerly called print2digits: 
  // http://arduino.cc/forum/index.php?topic=64024.30
  void convert_num(uint32_t num, byte digits, char* str) 
  {
    DEBUG_PRINTLN(F("Making this num legible:"),0);
    DEBUG_PRINTLN(num,0);
    strcpy(str,"000000000000"); // pre-set -- should this go in PROGMEM? 
                                // Experiment later - A: Probably not - I'm 
                                // going to manipulate it quite a bit every time
                                // this is called. It will wind up in memory as 
                                // STR anyway
    
    // i starts at the array index of the smallest digit in the substring, and
    // ends of at the array index of the largest digit in the substring
    for (char i = 11; i >= 1; i--)     {
      int digit = num % 10;
      str[i] = '0' + digit;
      num /= 10;
    }
    DEBUG_PRINTLN(str, 0);
    DEBUG_PRINT(F("Stripping digits to: ")); DEBUG_PRINTLN(digits,0);
    fit_digits(str, digits);
    DEBUG_PRINTLN(str, 0);
    DEBUG_PRINTLN(F("Done Converting Num"), 0);
  }
  
  //////////////////////////////
  // Convert Time - to a legible format
  //////////////////////////////
  // Consider making digits into an enum
  // Possibly add another function or another parameter to return strings of a 
  // certain length
  // String management reference:https://hackingmajenkoblog.wordpress.com/2016/02/04/the-evils-of-arduino-strings/
  //////////////////////////////
  
  void convert_time(uint32_t te, byte digits, char* str) 
  {
    // byte h, m, s // Can we store these and do UL math on them?
    DEBUG_PRINTLN(F("Making this time legible:"),0);
    DEBUG_PRINTLN(te,0);
    strcpy(str,"00:00:00.000"); //pre-set -- should this go in PROGMEM? 
                                //Experiment later - A: Probably not - I'm going
                                // to manipulate it quite a bit every time this 
                                //is called. It will wind up in memory as STR 
                                //anyway
  
    uint32_t tx; // Used to store the portion of time elapsed that matches the
                 // resolution(hours,mins,seconds) I am calculating
    uint32_t hdiv = 3600000;  //hours divisor
    uint32_t mdiv = 60000;  //minutes divisor
    uint32_t sdiv = 1000;  //seconds divisor
  
    // HOURS
    tx = te / hdiv;
    // The following prints the digits in order of ascending significance (i.e. 
    // units, then tens, etc.) up to a maximum of the number of digits I care to
    // keep: Based on this example: 
    // http://stackoverflow.com/questions/1397737/how-to-get-the-digits-of-a-number-without-converting-it-to-a-string-char-array
    // Make this a helper function?

    // i starts at the array index of the smallest digit in the substring, and
    // ends of at the array index of the largest digit in the substring
    for (char i = 1; i >= 0; i--) 
    {
      int digit = tx % 10;
      str[i] = '0' + digit;
      tx /= 10;
    }
    te %= hdiv; // or if modulo does not work: tx -= x * hdiv;
  
    // MINUTES
    tx = te / mdiv;
    // i starts at the array index of the smallest digit in the substring, and
    // ends of at the array index of the largest digit in the substring
    for (char i = 4; i >= 3; i--) 
    {
      int digit = tx % 10;
      str[i] = '0' + digit;
      tx /= 10;
    }
    te %= mdiv;
  
    // SECONDS
    tx = te / sdiv;
    // i starts at the array index of the smallest digit in the substring, and
    // ends of at the array index of the largest digit in the substring
    for (char i = 7; i >= 6; i--) 
    {
      int digit = tx % 10;
      str[i] = '0' + digit;
      tx /= 10;
    }
    te %= sdiv;
  
    // MILLISECONDS
    tx = te;
    // i starts at the array index of the smallest digit in the substring, and
    // ends of at the array index of the largest digit in the substring
    for (char i = 11; i >= 9; i--) 
    {
      int digit = tx % 10;
      str[i] = '0' + digit;
      tx /= 10;
    }
    DEBUG_PRINTLN(str, 0);
    DEBUG_PRINT(F("Stripping digits to: ")); DEBUG_PRINTLN(digits,0);
    fit_digits(str, digits);
    DEBUG_PRINTLN(str, 0);
    DEBUG_PRINTLN(F("Done Converting Time"), 0);
  }

#endif // LEGIBLETIME_H_