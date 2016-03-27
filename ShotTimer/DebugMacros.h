////////////////////////////////////////////////////////////
// DebugMacros
// Author: hestenet
// Canonical Repository: https://github.com/hestenet/arduino-shot-timer
// Ideas sourced from: http://forum.arduino.cc/index.php?topic=46900.0
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

#ifndef DEBUGMACROS_H_
  #define DEBUGMACROS_H_

  #ifdef DEBUG
    #include <Arduino.h>
    ////////////////////////////////////////////////////////////
    // DEBUG STRING CONSTANTS
    ////////////////////////////////////////////////////////////
    const char PROGMEM kMillisStr[] = "ms";
    const char PROGMEM kBytesStr[] = "B";
    const char PROGMEM kColStr[] = ":";
    const char PROGMEM kSpaceStr[] = " ";
    const char PROGMEM kNewlineStr[] = "\n";
    

    ////////////////////////////////////////////////////////////
    // INTERNAL FUNCTIONS
    ////////////////////////////////////////////////////////////
    
    //////////////////////////////
    // FreeRam() - Check available dynamic memory - 
    // http://playground.arduino.cc/Code/AvailableMemory
    //////////////////////////////

    int free_ram() {
      extern int __heap_start, *__brkval; 
      int v; 
      return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
    }
    
    //////////////////////////////
    // Serial PROGMEM Helpers - Print a string from PROGMEM to Serial output
    //////////////////////////////

    void serial_print_p(const char* kStr) {
      char c;
      if (!kStr)
      {
        return;
      }
      while (c = pgm_read_byte(kStr++))
      {
        Serial.print(c);
      }
    }
    //println
    void serial_println_p(const char* kStr) {
      char c;
      if (!kStr)
      {
        return;
      }
      while (c = pgm_read_byte(kStr++))
      {
        Serial.print(c);
      }
      serial_print_p(kNewlineStr); //Serial.print("\n");
    }

    ////////////////////////////////////////////////////////////
    // EXTERNAL MACROS 
    ////////////////////////////////////////////////////////////

    #define DEBUG_SETUP()                               \
      Serial.begin(9600);                               \
      DEBUG_PRINTLN(F("DEBUG is enabled."),0);          \
      DEBUG_PRINT(F("Comment out DEBUG and recompile"));\
      DEBUG_PRINTLN(F("to free PROGMEM and SRAM."),0);  
    #define DEBUG_PRINT(str)                            \
      Serial.print(str);
    #define DEBUG_PRINTLN(str, bl)                      \
      if (bl > 0) {                                     \
      serial_print_p(kMillisStr);                        \
      Serial.print(millis());                           \
      serial_print_p(kBytesStr);                         \
      Serial.print(FreeRam());                          \
      serial_print_p(kColStr);                           \
      Serial.print(__PRETTY_FUNCTION__);                \
      serial_print_p(kSpaceStr);                         \
      Serial.print(__FILE__);                           \
      serial_print_p(kColStr);                           \
      Serial.print(__LINE__);                           \
      serial_print_p(kSpaceStr);                         \
      }                                                 \
      Serial.println(str);
    #define DEBUG_PRINT_P(str)                          \
      serial_print_p(str);
    #define DEBUG_PRINTLN_P(str, bl)                    \
      if (bl > 0) {                                     \
      serial_print_p(kMillisStr);                        \
      Serial.print(millis());                           \
      serial_print_p(kBytesStr);                         \
      Serial.print(FreeRam());                          \
      serial_print_p(kColStr);                           \
      Serial.print(__PRETTY_FUNCTION__);                \
      serial_print_p(kSpaceStr);                         \
      Serial.print(__FILE__);                           \
      serial_print_p(kColStr);                           \
      Serial.print(__LINE__);                           \
      serial_print_p(kSpaceStr);                         \
      }                                                 \
      serial_println_p(str);
    
  #else
    #define DEBUG_SETUP()
    #define DEBUG_PRINT(str)
    #define DEBUG_PRINTLN(str, bl)
    #define DEBUG_PRINT_P(str)
    #define DEBUG_PRINTLN_P(str, bl)
  #endif

#endif // DEBUGMACROS_H_