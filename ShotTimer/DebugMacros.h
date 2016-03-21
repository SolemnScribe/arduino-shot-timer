/////////////////////////////////
// DebugMacros
// Author: hestenet
// Canonical Repository: https://github.com/hestenet/arduino-shot-timer
// Ideas sourced from: http://forum.arduino.cc/index.php?topic=46900.0
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

#ifndef DEBUGMACROS_H
  #define DEBUGMACROS_H

  #ifdef DEBUG
    #include <Arduino.h>
    //////////////////////
    // DEBUG STRING CONSTANTS
    //////////////////////
    const char PROGMEM millisStr[] = "ms";
    const char PROGMEM bytesStr[] = "B";
    const char PROGMEM colStr[] = ":";
    const char PROGMEM spaceStr[] = " ";
    const char PROGMEM newlineStr[] = "\n";
    //////////////////////
    //////////////////////
    // INTERNAL FUNCTIONS
    //////////////////////
    
    ///////////
    //FREE_RAM - Check available dynamic memory http://playground.arduino.cc/Code/AvailableMemory
    ///////////
    int FREE_RAM() //Checking Free RAM http://playground.arduino.cc/Code/AvailableMemory
    {
      extern int __heap_start, *__brkval; 
      int v; 
      return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
    }
    
    ///////////
    // Serial PROGMEM Helpers - Print a string from PROGMEM to Serial output
    ///////////
    //print
    void serialPrint_p(const char* str)
    {
      char c;
      if (!str)
      {
        return;
      }
      while (c = pgm_read_byte(str++))
      {
        Serial.print(c);
      }
    }
    //println
    void serialPrintln_p(const char* str)
    {
      char c;
      if (!str)
      {
        return;
      }
      while (c = pgm_read_byte(str++))
      {
        Serial.print(c);
      }
      serialPrint_p(newlineStr); //Serial.print("\n");
    }

    //////////////////////
    // EXTERNAL MACROS 
    //////////////////////
    #define DEBUG_SETUP()                 \
      Serial.begin(9600);                 \
      DEBUG_PRINTLN(F("DEBUG is enabled."),0); \
      DEBUG_PRINTLN(F("Comment out DEBUG and recompile to free PROGMEM and SRAM."),0);
    #define DEBUG_PRINT(str)              \
      Serial.print(str);
    #define DEBUG_PRINTLN(str, bl)        \
      if (bl > 0)                         \
      {                                   \
      serialPrint_p(millisStr);           \
      Serial.print(millis());             \
      serialPrint_p(bytesStr);            \
      Serial.print(FREE_RAM());           \
      serialPrint_p(colStr);              \
      Serial.print(__PRETTY_FUNCTION__);  \
      serialPrint_p(spaceStr);            \
      Serial.print(__FILE__);             \
      serialPrint_p(colStr);              \
      Serial.print(__LINE__);             \
      serialPrint_p(spaceStr);            \
      }                                   \
      Serial.println(str);
    #define DEBUG_PRINT_P(str)            \
      serialPrint_p(str);
    #define DEBUG_PRINTLN_P(str, bl)      \
      if (bl > 0)                         \
      {                                   \
      serialPrint_p(millisStr);           \
      Serial.print(millis());             \
      serialPrint_p(bytesStr);            \
      Serial.print(FREE_RAM());           \
      serialPrint_p(colStr);              \
      Serial.print(__PRETTY_FUNCTION__);  \
      serialPrint_p(spaceStr);            \
      Serial.print(__FILE__);             \
      serialPrint_p(colStr);              \
      Serial.print(__LINE__);             \
      serialPrint_p(spaceStr);            \
      }                                   \
      serialPrintln_p(str);
    
  #else
    #define DEBUG_SETUP()
    #define DEBUG_PRINT(str)
    #define DEBUG_PRINTLN(str, bl)
    #define DEBUG_PRINT_P(str)
    #define DEBUG_PRINTLN_P(str, bl)
  #endif

#endif


