/////////////////////////////////
// Shot Timer
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

//////////////////////////////////////////
// HARDWARE
// Arduino Uno R3
// Adafruit RGB LCD Shield - https://www.adafruit.com/products/714
// Adafruit Electet Mic/Amp - https://www.adafruit.com/products/1063
// Piezzo Buzzer
//////////////////////////////////////////

/////////////////////////////////////////
// Current Flaws:
// listenForShots(); probably could be redesigned to run on a timer interupt.
// as a result, the parTimes beep may come as early or late as half the sample window time.
// However, at most reasonable sampleWindows this will likely be indistinguishable to the user
//
// ParTimes are not saved to EEPROM, as their frequent updating is more likely to burn out the chip
//
// I would like to add SD card support to save the strings
//
// I would like to add course scoring of some kind, and maybe even shooter profiles, but the arduino Uno is likely not powerful enough for this.
//////////////////////////////////////////

//////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////

//////////////
//DEBUG
//////////////
#define DEBUG  //comment this out to disable debug information and remove all DEBUG messages at compile time
#include "DebugMacros.h"

//////////////
// Libraries - Core
// These are libraries shipped with Arduino, or that can be installed from the "Manage Libraries" interface of the Arduino IDE
// Sketch -> Include Libraries -> Manage Libraries
//////////////

//PROGMEM aka FLASH memory, non-volatile
#include <avr/pgmspace.h>
//PGMWrap makes it easier to interact with PROGMEM - declare data types with _p e.g: char_p
#include <PGMWrap.h>

//EEPROM additional non-volatile space 
#include <EEPROM.h>
//EEWrap allows you to read/write from EEPROM without special functions and without directly specifying EEPROM address space. 
#include <EEWrap.h> // the .update() method allows you to only update EEPROM if values have changed. 

//Wire library lets you manage I2C and 2 pin
#include <Wire.h>

//Chrono - LightChrono - chronometer - to replace StopWatch
#include <LightChrono.h>

//MenuSystem  //NOTE: Version 2.0.2 recommended - the 2.1.0 release breaks this code! Not yet sure why. 
#include <MenuSystem.h>

//Adafruit RGB LCD Shield Library
#include <Adafruit_RGBLCDShield.h>


//////////////
// Libraries - Other
// These are libraries that cannot be found in the defauilt Arduino library manager - however, they can be added manually.
// A source url for each library is provided - simply download the library and include in:
// ~/Documents/Arduino/Libraries
//////////////

//toneAC
//Bit-Bang tone library for piezo buzzer https://bitbucket.org/teckel12/arduino-toneac/wiki/Home#!difference-between-toneac-and-toneac2
#include <toneAC.h>

//////////////
// Other code samples used:
//////////////
// Adafruit sound level sampling: http://learn.adafruit.com/adafruit-microphone-amplifier-breakout/measuring-sound-levels

//////////////
// Libraries - Mine
//////////////

//Tones for buttons and buzzer
#include "Pitches.h" // musical pitches - optional - format: NOTE_C4 | This include makes no difference to program or dynamic memory
//Convert time in ms elapsed to hh:mm:ss.mss
#include "LegibleTime.h"
//Helper functions for managing the LCD Display
#include "LCDHelpers.h"


//////////////
// CONSTANTS
//////////////
//////////////
// PROGMEM
//////////////

//Menu Names - format: const char Name[] PROGMEM = ""; <---- this doesn't work with menuBackEnd and I don't know why.
//To read these - increment over the array - https://github.com/Chris--A/PGMWrap/blob/master/examples/advanced/use_within_classes/use_within_classes.ino 
//More detailed example of dealing with strings and arrays in PROGMEM http://www.gammon.com.au/progmem
const char PROGMEM mainName[] = "Shot Timer v.3";
const char PROGMEM startName[] = "[Start]";
const char PROGMEM reviewName[] = "[Review]";
const char PROGMEM parName[] = "Set Par >>";
const char PROGMEM parSetName[] = "<< [Toggle Par]";
const char PROGMEM parTimesName[] = "<< [Par Times]";
const char PROGMEM settingsName[] = "Settings >>";
const char PROGMEM setDelayName[] = "<< [Set Delay]";
const char PROGMEM buzzerName[] = "<< [Buzzr Vol]";
const char PROGMEM sensitivityName[] = "<< [Sensitivity]";
const char PROGMEM echoName[] = "<< [Echo Reject]";

const uint8_p PROGMEM micPin = A0; // the mic/amp is connected to analog pin 0
//set the input for the mic/amplifier

// set attributes for the button tones
const uint8_p PROGMEM buttonVol = 5;
const uint8_p PROGMEM buttonDur = 80;

const int16_p PROGMEM beepDur = 400;
const int16_p PROGMEM beepNote = NOTE_C4;


//////////////
// Instantiation
//////////////

// StopWatch
//StopWatch shotTimer;
LightChrono shotChrono;

// The shield uses the I2C SCL and SDA pins. On classic Arduinos
// this is Analog 4 and 5 so you can't use those for analogRead() anymore
// However, you can connect other I2C sensors to the I2C bus and share
// the I2C bus.
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

////////////////////////////////////////
// EEPROM: Settings to be stored in EEPROM
////////////////////////////////////////
// EEPROM HAS 100,000 READ/WRITE CYCLES, conservatively
// http://tronixstuff.wordpress.com/2011/05/11/discovering-arduinos-internal-eeprom-lifespan/
uint8_e delaySetting;  // Can be 0
uint8_e beepSetting;  // Can be 0
uint8_e sensSetting;  // Can be 0
uint8_e sampleSetting; //Cannot be 0  //  ECHO REJECT: Sample window width in mS (50 mS = 20Hz) for function sampleSound()
// Global variables stoired in EEProm can not be initialized
// The solution is to declare them uninitialized and then call a setup function based on an if condition...
// If one of the values is set to 0/null that is not allowed to be set to 0/null than the EEPROM should be updated to the default values. 
// Or alternately - if all 4 values are set to 0/null than the EEPROM clearly hasn't been set. 
byte delayTime = 11;
byte beepVol = 10;
byte sensitivity = 1;
byte sampleWindow = 50;
/////////////////////////////////////////

//////////////
// GLOBAL VARIABLES
//////////////
unsigned long shotTimes[200];
unsigned long parTimes[10];
unsigned long additivePar;
byte currentShot = 0;
byte reviewShot = 0;
byte currentPar = 0;
int threshold = 625; //The sensitivity setting is converted into a threshold value
byte parCursor = 1;
boolean parEnabled = false;


///////////////
// Program State Variables
///////////////
byte buttonState;
byte programState; // see if we can refactor our booleans below to use this single programState byte 
  // 0 - Navigating menus
  // 1 - Timer is running
  // 2 - Reviewing shots
  // 3 - Setting Par State
  // 4 - Setting Par Times
  // 5 - ?? Editing Par
  // 6 - Setting Delay
  // 7 - Setting Beep
  // 8 - Setting Sensitivity 
  // 9 - Setting Echo
  // +100 = parEnabled
boolean isRunning = 0;
boolean reviewingShots = false;


boolean settingParState = false;
boolean settingParTimes = false;
boolean editingPar = false;
boolean settingDelay = false;
boolean settingBeep = false;
boolean settingSensitivity = false;
boolean settingEcho = false;


//////////////
//Menus and Menu Items
//////////////

MenuSystem tm;
Menu mainMenu(mainName);
  MenuItem menuStart(startName);
  MenuItem menuReview(reviewName);
  Menu parMenu(parName);
    MenuItem menuParState(parSetName);
    MenuItem menuParTimes(parTimesName);
  Menu settingsMenu(settingsName);
    MenuItem menuStartDelay(setDelayName);
    MenuItem menuBuzzer(buzzerName);
    MenuItem menuSensitivity(sensitivityName);
    MenuItem menuEcho(echoName);


//////////////
// FUNCTIONS
//////////////


//////////////////////////////////////////////////////////
// Render the current menu screen
//////////////////////////////////////////////////////////

void renderMenu() {
  Menu const* menu = tm.get_current_menu();
  //const char* menu1_name = menu->get_selected()->get_name();
  //const char* menu_name = menu->get_selected()->get_name();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcdPrint_p(&lcd, menu->get_name()); // lcd.print(F("Shot Timer v.3"));
  DEBUG_PRINT(F("Rendering Menu: "));
  DEBUG_PRINTLN_P(menu->get_name(),0);
  lcd.setCursor(0, 1);
  lcdPrint_p(&lcd, menu->get_selected()->get_name());
  DEBUG_PRINT(F("Rendering Item: "));
  DEBUG_PRINTLN_P(menu->get_selected()->get_name(),0);
}

//////////////////////////////////////////////////////////
// Sample Sound
//////////////////////////////////////////////////////////
int sampleSound() {
  unsigned long startMillis = millis(); // Start of sample window --- the peak to peak reading
  // will be the total loudness change across the sample wiindow!
  int peakToPeak = 0; // peak-to-peak level
  int sample = 0;
  int signalMax = 0;
  int signalMin = 1024;

  // collect data for duration of sampleWindow
  while (millis() - startMillis < sampleWindow)
  {
    sample = analogRead(micPin);
    if (sample < 1024) // toss out spurious readings
    {
      if (sample > signalMax)
      {
        signalMax = sample; // save just the max levels
      }
      else if (sample < signalMin)
      {
        signalMin = sample; // save just the min levels
      }
    }
  }
  peakToPeak = signalMax - signalMin; // max - min = peak-peak amplitude
  //int millVolts = ((peakToPeak * 3.3) / 1024)*1000 ; // convert to millivolts
  return (peakToPeak);
}

//////////////////////////////////////////////////////////
// Listen for Shots
//////////////////////////////////////////////////////////

void listenForShots() {
  if (sampleSound() >= threshold) {
    recordShot();
  }
}

//////////////////////////////////////////////////////////
// Start the Shot Timer
//////////////////////////////////////////////////////////
// Consider changing these to be 'on_menu_event()' functions - such that they can have a local variable for whether the menu item is active, rather than using a global. 
// Also - transition menus and re-render menu screens in the stop condition. 
void on_menuStart_selected(MenuItem* p_menu_item) {
  isRunning = 1;
  lcd.setBacklight(GREEN);
  //shotTimer.restart(); //reset the timer to 0
  for (int c = 0; c < currentShot; c++) { // reset the values of the array of shots to 0 NOT <= because currentShot is incremented at the end of the last one recorded
    shotTimes[c] = 0;
  }
  currentShot = 0; //start with the first shot in the array
  lcd.setCursor(0, 0);
  lcd.print(F("Wait for it...  "));
  lcd.setCursor(0, 1);
  lcd.print(F("                ")); // create a clearline function? Save fewer strings in progmem?
  startDelay();
  lcd.setCursor(0, 0);
  lcd.print(F("      GO!!      "));
  lcd.setCursor(0, 1);
  lcd.print(F("Last:")); //10 chars
  BEEP();
  //shotTimer.start();
  
  shotChrono.restart();
  //serialPrintln(shotTimer.elapsed(), 7);
  convertTime(shotChrono.elapsed(), 7, NULL); // for DEBUG
}
//////////////////////////////////////////////////////////
// Stop the shot timer
//////////////////////////////////////////////////////////
void stopTimer(boolean out = 0) {
  isRunning = 0;
  if (out == 1) {
    lcd.setBacklight(RED);
  }
  else {
    lcd.setBacklight(WHITE);
  }
  //shotTimer.stop();
  DEBUG_PRINTLN(F("Timer was stopped at:"), 0);
  convertTime(shotChrono.elapsed(), 7, NULL); // for DEBUG
  for (int i = 0; i < 5; i++) {
    toneAC(beepNote, 9, 100, false);
    delay(50);
  }
  if (out == 1) {
    lcd.setBacklight(WHITE);
  }
  tm.next(); //move the menu down to review mode
  tm.select(); //move into shot review mode immediately
}

//////////////////////////////////////////////////////////
// record shots to the string array
//////////////////////////////////////////////////////////

void recordShot() {
  //shotTimes[currentShot] = shotTimer.elapsed();
  shotTimes[currentShot] = shotChrono.elapsed();
  DEBUG_PRINT(F("Shot #")); DEBUG_PRINT(currentShot + 1); DEBUG_PRINT(F(" - "));// serialPrintln(shotTimes[currentShot], 7);
  DEBUG_PRINT(F("\n"));
  //serialPrintln(shotTimer.elapsed());
  //serialPrintln(shotChrono.elapsed(), 7);
  lcd.setCursor(6, 1);
  lcdPrintTime(&lcd, shotTimes[currentShot], 7); //lcd.print(F(" ")); if(currentShot > 1) {lcdPrintTime(&lcd, shotTimes[currentShot]-shotTimes[currentShot-1],5);}
  //9 characters             //1 characters                    //6 characters
  currentShot += 1;
  if (currentShot == sizeof(shotTimes)) { // if the current shot == 100 (1 more than the length of the array)
    stopTimer(1);
  }
}
//////////////////////////////////////////////////////////
//review shots - initialize the shot review screen
//////////////////////////////////////////////////////////

void on_menuReview_selected(MenuItem* p_menu_item) {
  reviewingShots = !reviewingShots;
  if (reviewingShots == 1) {
    if (currentShot > 0) {
      reviewShot = currentShot - 1;
    }
    DEBUG_PRINT(reviewShot);
    //DEBUG FOR LOOP - PRINT ALL SHOT TIMES IN THE STRING TO SERIAL 
    for (int t = 0; t < currentShot; t++) {
      DEBUG_PRINT(F("Shot #"));
      DEBUG_PRINT(t + 1);
      DEBUG_PRINT(F(" - "));
      convertTime(shotTimes[t], 7, NULL); // for DEBUG
    }
    lcd.setBacklight(VIOLET);
    lcd.setCursor(0, 0);
    lcd.print(F("Shot #"));
    lcd.print(currentShot);
    lcd.print(F("                "));
    lcd.setCursor(9, 0);
    lcd.print(F(" Split "));
    lcd.setCursor(0, 1);
    lcdPrintTime(&lcd, shotTimes[reviewShot], 7);
    lcd.print(F(" "));
    if (reviewShot > 1) {
      lcdPrintTime(&lcd, shotTimes[reviewShot] - shotTimes[reviewShot - 1], 5);
    }
    //9 characters             //1 characters                    //6 characters
  }
  else {
    lcd.setBacklight(WHITE);
    renderMenu();
  }
  DEBUG_PRINTLN(reviewingShots, 0);
}

//////////////////////////////////////////////////////////
// review shots - move to the next shot in the string
//////////////////////////////////////////////////////////

void nextShot() {
  if (currentShot == 0 || reviewShot == currentShot - 1) {
    reviewShot = 0;
  }
  else {
    reviewShot++;
  }
  lcd.setCursor(0, 0);
  lcd.print(F("Shot #"));
  lcd.print(reviewShot + 1);
  lcd.print(F("                "));
  lcd.setCursor(9, 0);
  lcd.print(F(" Split "));
  lcd.setCursor(0, 1);
  lcdPrintTime(&lcd, shotTimes[reviewShot], 7);
  lcd.print(F(" "));
  if (reviewShot == 0) {
    lcd.print(F("   1st"));
  }
  else {
    lcdPrintTime(&lcd, shotTimes[reviewShot] - shotTimes[reviewShot - 1], 5);
  }
}

//////////////////////////////////////////////////////////
// review shots - move to the previous shot in the string
//////////////////////////////////////////////////////////

void previousShot() {
  if (currentShot == 0) {
    reviewShot = 0;
  }
  else if (reviewShot == 0) {
    reviewShot = currentShot - 1;
  }
  else {
    reviewShot--;
  }

  lcd.setCursor(0, 0);
  lcd.print(F("Shot #"));
  lcd.print(reviewShot + 1);
  lcd.print(F("                "));
  lcd.setCursor(9, 0);
  lcd.print(F(" Split "));
  lcd.setCursor(0, 1);
  lcdPrintTime(&lcd, shotTimes[reviewShot], 7);
  lcd.print(F(" "));
  if (reviewShot == 0) {
    lcd.print(F("   1st"));
  }
  else {
    lcdPrintTime(&lcd, shotTimes[reviewShot] - shotTimes[reviewShot - 1], 5);
  }
}

//////////////////////////////////////////////////////////
// Rate of Fire
//////////////////////////////////////////////////////////

void rateOfFire(boolean includeDraw = true) {
  unsigned int rof;
  if (!includeDraw) {
    rof = (shotTimes[currentShot - 1] - shotTimes[0]) / (currentShot - 1);
  }
  else rof = shotTimes[currentShot - 1] / (currentShot - 1);

  lcd.setCursor(0, 0);
  lcd.print(F("                "));
  lcd.setCursor(0, 0);
  lcd.print(F("Avg Split:"));
  lcd.setCursor(11, 0);
  lcdPrintTime(&lcd, rof, 5);
  lcd.setCursor(0, 1);
  lcd.print(F("                "));
  lcd.setCursor(0, 1);
  lcd.print(F("Shots/min:"));
  lcd.setCursor(11, 1);
  lcd.print(60000 / rof); // will this produce a decimal? Or a truncated int?
  //10 characters   //1 characters          //4 characters plus truncated?
}


/////////////////////////////////////////////////////////////
// on_menuStartDelay_selected
/////////////////////////////////////////////////////////////

void on_menuStartDelay_selected(MenuItem* p_menu_item) {
  settingDelay = !settingDelay;
  if (settingDelay == 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Start Delay"));
    lcd.setCursor(0, 1);
    if (delayTime > 11) {
      lcd.print(F("Random 2-6s"));
    }
    else if (delayTime == 11) {
      lcd.print(F("Random 1-4s"));
    }
    else {
      lcd.print(delayTime);
    }
  }
  else {
    delaySetting = delayTime;
    renderMenu();
  }
  DEBUG_PRINTLN(settingDelay, 0);
}


/////////////////////////////////////////////////////////////
// increaseDelay
/////////////////////////////////////////////////////////////

void increaseDelay() {
  if (delayTime == 12) {
    delayTime = 0;
  }
  else {
    delayTime++;
  }
  lcd.setCursor(0, 1);
  if (delayTime > 11) {
    lcd.print(F("Random 2-6s"));
  }
  else if (delayTime == 11) {
    lcd.print(F("Random 1-4s"));
  }
  else {
    lcd.print(delayTime);
  }
  lcd.print(F("                "));
}

/////////////////////////////////////////////////////////////
// decreaseDelay
/////////////////////////////////////////////////////////////

void decreaseDelay() {
  if (delayTime == 0) {
    delayTime = 12;
  }
  else {
    delayTime--;
  }
  lcd.setCursor(0, 1);
  if (delayTime > 11) {
    lcd.print(F("Random 2-6s"));
  }
  else if (delayTime == 11) {
    lcd.print(F("Random 1-4s"));
  }
  else {
    lcd.print(delayTime);
  }
  lcd.print(F("                "));
}

/////////////////////////////////////////////////////////////
// startDelay
/////////////////////////////////////////////////////////////

void startDelay() {
  if (delayTime > 11) {
    delay(random(2000, 6001)); //from 2 to 6 seconds
  }
  else if (delayTime == 11) {
    delay(random(1000, 4001)); //from 1 to 4 seconds
  }
  else {
    delay(delayTime * 1000); //fixed number of seconds
  }
}


/////////////////////////////////////////////////////////////
// on_menuBuzzer_selected
/////////////////////////////////////////////////////////////

void on_menuBuzzer_selected(MenuItem* p_menu_item) {
  settingBeep = !settingBeep;
  if (settingBeep == 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Buzzer Volume"));
    lcd.setCursor(0, 1);
    lcdPrint(&lcd, beepVol, 2);
  }
  else {
    beepSetting = beepVol;
    renderMenu();
  }
  DEBUG_PRINTLN(settingBeep, 0);
}


/////////////////////////////////////////////////////////////
// increaseBeepVol
/////////////////////////////////////////////////////////////

void increaseBeepVol() {
  if (beepVol == 10) {
    beepVol = 0;
  }
  else {
    beepVol++;
  }
  lcd.setCursor(0, 1);
  lcdPrint(&lcd, beepVol, 2);
  lcd.print(F("                ")); //TODO REplace with a single PROGMEM clear buffer
}

/////////////////////////////////////////////////////////////
// decreaseBeepVol
/////////////////////////////////////////////////////////////

void decreaseBeepVol() {
  if (beepVol == 0) {
    beepVol = 10;
  }
  else {
    beepVol--;
  }
  lcd.setCursor(0, 1);
  lcdPrint(&lcd, beepVol, 2);
  lcd.print(F("                "));
}


/////////////////////////////////////////////////////////////
// on_menuSensitivity_selected
/////////////////////////////////////////////////////////////

void on_menuSensitivity_selected(MenuItem* p_menu_item) {
  settingSensitivity = !settingSensitivity;
  if (settingSensitivity == 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Sensitivity"));
    lcd.setCursor(0, 1);
    lcdPrint(&lcd, sensitivity, 2);
  }
  else {
    sensSetting = sensitivity;
    renderMenu();
  }
  DEBUG_PRINTLN(settingSensitivity, 0);
}


/////////////////////////////////////////////////////////////
// increaseSensitivity
/////////////////////////////////////////////////////////////

void increaseSensitivity() {
  if (sensitivity == 20) {
    sensitivity = 0;
  }
  else {
    sensitivity++;
  }
  sensToThreshold();
  lcd.setCursor(0, 1);
  lcdPrint(&lcd, sensitivity, 2);
  lcd.print(F("                "));
}

/////////////////////////////////////////////////////////////
// decreaseSensitivity
/////////////////////////////////////////////////////////////

void decreaseSensitivity() {
  if (sensitivity == 0) {
    sensitivity = 20;
  }
  else {
    sensitivity--;
  }
  sensToThreshold();
  lcd.setCursor(0, 1);
  lcdPrint(&lcd, sensitivity, 2);
  lcd.print(F("                "));
}


/////////////////////////////////////////////////////////////
// on_menuEcho_selected - EEPROM
/////////////////////////////////////////////////////////////

void on_menuEcho_selected(MenuItem* p_menu_item) {
  settingEcho = !settingEcho;
  if (settingEcho == 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Echo Protect"));
    lcd.setCursor(0, 1);
    lcd.print(sampleWindow);
    lcd.print(F("ms"));
  }
  else {
    sampleSetting = sampleWindow;
    renderMenu();
  }
  DEBUG_PRINTLN(settingEcho, 0);
}


/////////////////////////////////////////////////////////////
// increaseEchoProtect
/////////////////////////////////////////////////////////////

void increaseEchoProtect() {
  if (sampleWindow == 100) {
    sampleWindow = 10;
  }
  else {
    sampleWindow += 10;
  }
  lcd.setCursor(0, 1);
  lcd.print(sampleWindow);
  lcd.print(F("ms              ")); //CLEARLINE
}

/////////////////////////////////////////////////////////////
// decreaseEchoProtect
/////////////////////////////////////////////////////////////

void decreaseEchoProtect() {
  if (sampleWindow == 10) {
    sampleWindow = 100;
  }
  else {
    sampleWindow -= 10;
  }
  lcd.setCursor(0, 1);
  lcd.print(sampleWindow);
  lcd.print(F("ms              "));//CLEARLINE
}

/////////////////////////////////////////////////////////////
// convert sensitivity to threshold
/////////////////////////////////////////////////////////////

void sensToThreshold() {
  threshold = 650 - (25 * sensitivity);
}


/////////////////////////////////////////////////////////////
// on_menuParState_selected
/////////////////////////////////////////////////////////////

void on_menuParState_selected(MenuItem* p_menu_item) {
  settingParState = !settingParState;
  if (settingParState == 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Par Times"));
    lcd.setCursor(0, 1);
    if (parEnabled == false) {
      lcd.print(F("[DISABLED]"));
    }
    else {
      lcd.print(F("[ENABLED]"));
    }
  }
  else {
    renderMenu();
  }
  DEBUG_PRINTLN(settingParState, 0);
}


/////////////////////////////////////////////////////////////
// toggleParState
/////////////////////////////////////////////////////////////

void toggleParState() {
  parEnabled = !parEnabled;
  lcd.setCursor(0, 1);
  if (parEnabled == false) {
    lcd.print(F("[DISABLED]")); //10 characters
  }
  else {
    lcd.print(F("[ENABLED] ")); //10 characters
  }
}

/////////////////////////////////////////////////////////////
// on_menuParTimes_selected
/////////////////////////////////////////////////////////////

void on_menuParTimes_selected(MenuItem* p_menu_item) {
  settingParTimes = !settingParTimes;
  if (settingParTimes == 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("<<"));
    lcd.setCursor(5, 0);
    lcd.print(F("Par"));
    lcd.setCursor(9, 0);
    lcdPrint(&lcd, (currentPar + 1), 2);
    lcd.setCursor(4, 1);
    if (currentPar > 0) {
      lcd.print(F("+"));
    }
    else {
      lcd.print(F(" "));
    }
    lcdPrintTime(&lcd, parTimes[currentPar], 7);
  }
  else {
    renderMenu();
  }
  DEBUG_PRINTLN(settingParState, 0);
}


/////////////////////////////////////////////////////////////
// parUp()
/////////////////////////////////////////////////////////////

void parUp() {
  if (currentPar == 0) {
    currentPar = sizeof(parTimes) - 1;
  }
  else {
    currentPar--;
  }
  lcd.setCursor(9, 0);
  lcdPrint(&lcd, (currentPar + 1), 2);
  lcd.setCursor(4, 1);
  if (currentPar > 0) {
    lcd.print(F("+"));
  }
  else {
    lcd.print(F(" "));
  }
  lcdPrintTime(&lcd, parTimes[currentPar], 7);
}

/////////////////////////////////////////////////////////////
// parDown()
/////////////////////////////////////////////////////////////

void parDown() {
  if (currentPar == sizeof(parTimes) - 1) {
    currentPar = 0;
  }
  else {
    currentPar++;
  }
  lcd.setCursor(9, 0);
  lcdPrint(&lcd, (currentPar + 1), 2);
  lcd.setCursor(4, 1);
  if (currentPar > 0) {
    lcd.print(F("+"));
  }
  else {
    lcd.print(F(" "));
  }
  lcdPrintTime(&lcd, parTimes[currentPar], 7);
}


/////////////////////////////////////////////////////////////
// editPar()
/////////////////////////////////////////////////////////////

void editPar() {
  settingParTimes = 0;
  editingPar = !editingPar;
  if (editingPar == 1) {
    lcd.setBacklight(GREEN);
    lcd.setCursor(0, 0);
    lcd.print(F("Edit        "));
    lcd.setCursor(0, 1);
    lcd.print(F("P"));
    lcdPrint(&lcd, currentPar + 1, 2);
    if (currentPar > 0) {
      lcd.print(F(" +"));
    }
    else {
      lcd.print(F("  "));
    }
    parCursor = 4; //reset cursor to the seconds position
    lcdCursor();
  }
  else {
    lcd.setBacklight(WHITE);
    tm.select();
  }
  DEBUG_PRINTLN(editingPar, 0);
}

/////////////////////////////////////////////////////////////
// leftCursor()
/////////////////////////////////////////////////////////////

void leftCursor() {
  if (parCursor == 7) {
    parCursor = 1;
  }
  else {
    parCursor++;
  }
  lcdCursor();
}

/////////////////////////////////////////////////////////////
// rightCursor()
/////////////////////////////////////////////////////////////

void rightCursor() {
  if (parCursor == 1) {
    parCursor = 7;
  }
  else {
    parCursor--;
  }
  lcdCursor();
}

/////////////////////////////////////////////////////////////
// lcdCursor()
/////////////////////////////////////////////////////////////
//switch case for cursor position displayed on screen
void lcdCursor() {
  switch (parCursor) {
    case 1: //milliseconds
      lcd.setCursor(11, 0); //icon at 13
      lcd.print(F("  _"));
      lcd.setCursor(5, 0); //left behind icon at 5
      lcd.print(F(" "));
      break;
    case 2: //ten milliseconds
      lcd.setCursor(10, 0); //icon at 12
      lcd.print(F("  _  "));
      break;
    case 3: //hundred milliseconds
      lcd.setCursor(9, 0); //icon at 11
      lcd.print(F("  _  "));
      break;
    case 4: //seconds
      lcd.setCursor(7, 0); //icon at 9
      lcd.print(F("  _  "));
      break;
    case 5: //ten seconds
      lcd.setCursor(6, 0); // icon at 8
      lcd.print(F("  _  "));
      break;
    case 6: //minutes
      lcd.setCursor(4, 0); //icon at 6
      lcd.print(F("  _  "));
      break;
    case 7: //ten minutes
      lcd.setCursor(5, 0); //icon at 5
      lcd.print(F("_  "));
      lcd.setCursor(13, 0); //left behind icon at 13
      lcd.print(F(" "));
      break;
  }
}

/////////////////////////////////////////////////////////////
// increaseTime()
/////////////////////////////////////////////////////////////

void increaseTime() {
  switch (parCursor) {
    case 1: // milliseconds
      if (parTimes[currentPar] == 5999999) {
        break;
      }
      else {
        parTimes[currentPar]++;
      }
      break;
    case 2: // hundreds milliseconds
      if (parTimes[currentPar] >= 5999990) {
        break;
      }
      else {
        parTimes[currentPar] += 10;
      }
      break;
    case 3: // tens milliseconds
      if (parTimes[currentPar] >= 5999900) {
        break;
      }
      else {
        parTimes[currentPar] += 100;
      }
      break;
    case 4: // seconds
      if (parTimes[currentPar] >= 5999000) {
        break;
      }
      else {
        parTimes[currentPar] += 1000;
      }
      break;
    case 5: // ten seconds
      if (parTimes[currentPar] >= 5990000) {
        break;
      }
      else {
        parTimes[currentPar] += 10000;
      }
      break;
    case 6: // minutes
      if (parTimes[currentPar] >= 5940000) {
        break;
      }
      else {
        parTimes[currentPar] += 60000;
      }
      break;
    case 7: // 10 minutes
      if (parTimes[currentPar] >= 5400000) {
        break;
      }
      else {
        parTimes[currentPar] += 600000;
      }
      break;
  }
  lcd.setCursor(5, 1);
  lcdPrintTime(&lcd, parTimes[currentPar], 7);
}

/////////////////////////////////////////////////////////////
// decreaseTime()
/////////////////////////////////////////////////////////////

void decreaseTime() {
  switch (parCursor) {
    case 1:
      if (parTimes[currentPar] < 1) {
        break;
      }
      else {
        parTimes[currentPar]--;
      }
      break;
    case 2:
      if (parTimes[currentPar] < 10) {
        break;
      }
      else {
        parTimes[currentPar] -= 10;
      }
      break;
    case 3:
      if (parTimes[currentPar] < 100) {
        break;
      }
      else {
        parTimes[currentPar] -= 100;
      }
      break;
    case 4:
      if (parTimes[currentPar] < 1000) {
        break;
      }
      else {
        parTimes[currentPar] -= 1000;
      }
      break;
    case 5:
      if (parTimes[currentPar] < 10000) {
        break;
      }
      else {
        parTimes[currentPar] -= 10000;
      }
      break;
    case 6:
      if (parTimes[currentPar] < 60000) {
        break;
      }
      else {
        parTimes[currentPar] -= 60000;
      }
      break;
    case 7:
      if (parTimes[currentPar] < 600000) {
        break;
      }
      else {
        parTimes[currentPar] -= 600000;
      }
      break;
  }
  lcd.setCursor(5, 1);
  lcdPrintTime(&lcd, parTimes[currentPar], 7);
}

/////////////////////////////////////////////////////////////
// BEEP
/////////////////////////////////////////////////////////////

void BEEP() {
  toneAC(beepNote, beepVol, beepDur, true);
}

/////////////////////////////////////////////////////////////
// buttonTone
/////////////////////////////////////////////////////////////

void buttonTone() {
  toneAC(beepNote, buttonVol, buttonDur, true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// SETUP FUNCTIONS
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////
// eepromSetup
// Note - EEWrap automatically uses an .update() on EEPROM writes, to avoid wearing out the EEPROM if the value being set is the same as the existing value. 
/////////////////////////////////////////////////////////////

void eepromSetup() {
  DEBUG_PRINTLN(F("Checking if EEPROM needs to be set..."), 0);

  // Use settings values from EEPROM only if the if non-null values have been set
  // Because 255 is the default for unused EEPROM and not a valid value for Sample Window...
  // if ANY of our EEPROM stored settings come back 255, we'll know that the EEPROM settings have not been set
  // By checking all 4 settings, we help ensure that legacy EEPROM data doesn't slip in and cause unexpected behavior.
  if (sampleSetting == 255 || sensSetting == 255 || beepSetting == 255 || delaySetting == 255) {
    DEBUG_PRINTLN(F("Setting EEPROM"), 0);
    delaySetting = delayTime;
      DEBUG_PRINTLN(F("Set delaySetting to "), 0);
      DEBUG_PRINTLN(delayTime, 0);
    beepSetting = beepVol;
      DEBUG_PRINTLN(F("Set beepSetting to "), 0);
      DEBUG_PRINTLN(beepVol, 0);
    sensSetting = sensitivity;
      DEBUG_PRINTLN(F("Set sensSetting to "), 0);
      DEBUG_PRINTLN(sensitivity, 0);
    sampleSetting = sampleWindow;
      DEBUG_PRINTLN(F("Set sampleSetting to "), 0);
      DEBUG_PRINTLN(sampleWindow, 0);
  }
  else {
    DEBUG_PRINTLN(F("Reading settings from EEPROM)"), 0);
    delayTime = delaySetting;
      DEBUG_PRINTLN(F("Set delayTime to "), 0);
      DEBUG_PRINTLN(delayTime, 0);
    beepVol = beepSetting;
      DEBUG_PRINTLN(F("Set beepVol to "), 0);
      DEBUG_PRINTLN(beepVol, 0);
    sensitivity = sensSetting;
      DEBUG_PRINTLN(F("Set sensitivity to "), 0);
      DEBUG_PRINTLN(sensitivity, 0);
    sampleWindow = sampleSetting;
      DEBUG_PRINTLN(F("Set sampleWindow to "), 0);
      DEBUG_PRINTLN(sampleWindow, 0);
  }
  sensToThreshold(); //make sure that the Threshold is calculated based on the stored sensitivity setting
}


//////////////////////////////////////////////////////////
// Menu Setup
//////////////////////////////////////////////////////////

void menuSetup()
{
  DEBUG_PRINTLN(F("Setting up menu:"),0);
  DEBUG_PRINTLN_P(mainName,0);
  mainMenu.add_item(&menuStart, &on_menuStart_selected);
  DEBUG_PRINTLN_P(startName,0);
  mainMenu.add_item(&menuReview, &on_menuReview_selected);
  DEBUG_PRINTLN_P(reviewName,0);
  mainMenu.add_menu(&parMenu);
  DEBUG_PRINTLN_P(parName,0);
    parMenu.add_item(&menuParState, &on_menuParState_selected);
    DEBUG_PRINTLN_P(parSetName,0);
    parMenu.add_item(&menuParTimes, &on_menuParTimes_selected);
    DEBUG_PRINTLN_P(parTimesName,0);
  mainMenu.add_menu(&settingsMenu);
  DEBUG_PRINTLN_P(settingsName,0);
    settingsMenu.add_item(&menuStartDelay, &on_menuStartDelay_selected);
    DEBUG_PRINTLN_P(setDelayName,0);
    settingsMenu.add_item(&menuBuzzer, &on_menuBuzzer_selected);
    DEBUG_PRINTLN_P(buzzerName,0);
    settingsMenu.add_item(&menuSensitivity, &on_menuSensitivity_selected);
    DEBUG_PRINTLN_P(sensitivityName,0);
    settingsMenu.add_item(&menuEcho, &on_menuEcho_selected);
    DEBUG_PRINTLN_P(echoName,0);
  tm.set_root_menu(&mainMenu); 
  DEBUG_PRINTLN(F("Menu Setup Complete"),0);
}

//////////////////////////////////////////////////////////
// LCD Setup
//////////////////////////////////////////////////////////

void lcdSetup() {
  lcd.begin(16, 2);
  lcd.setBacklight(WHITE);
  renderMenu();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//SETUP AND LOOP
////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////
// SETUP
//////////////

void setup() {
  DEBUG_SETUP();
  
  randomSeed(analogRead(1));
  
  //eepromSetup();

  menuSetup();

  lcdSetup();

  DEBUG_PRINTLN(F("Setup Complete"), 0);
}

//////////////
// LOOP
//////////////

void loop() {
  // DEBUG: compare result of sampleSound() vs the raw input
  // debugAudioInput();

  //only accept newly changed button states
  uint8_t newButtons = lcd.readButtons();
  uint8_t buttons = newButtons & ~buttonState;
  buttonState = newButtons;


  if (isRunning == 1) { //listening for shots
    listenForShots();

    if (parEnabled == 1) {
      //EXTRACT INTO A parBeeps() FUNCTION 
      additivePar = 0;
      for (byte i = 0; i < sizeof(parTimes); i++) {
        if (parTimes[i] == 0) {
          break;
        }
        additivePar += parTimes[i]; // add the parTimes together
        //if (shotTimer.elapsed() <= (additivePar + (sampleWindow / 2)) && shotTimer.elapsed() >= (additivePar - sampleWindow / 2)){
        int timeElapsed = shotChrono.elapsed();
        if (timeElapsed <= (additivePar + (sampleWindow / 2)) && timeElapsed >= (additivePar - sampleWindow / 2)) {
          BEEP();  //Beep if the current time matches (within the boundaries of sample window) the parTime
        }
      }

    }
  }

//CONSIDER - BREAK THESE MANY BUTTON STATEMENTS INTO A SWITCH CASE BASED ON PROGRAM STATE
//WITHIN EACH CASE HAVE A SINGLE BUTTON MANAGER FUNCTION FOR EACH STATE
//ALTERNATELY USE A SINGLE BUTTON MANAGER FUNCTION THAT SWITCHES BASED ON STATE 

  if (buttons) {
    if (isRunning == 1) { //while timer is running
      if (buttons & BUTTON_SELECT) {
        stopTimer();
      }
    }

    else  if (reviewingShots == 1) { //reviewing shots    /replace booleans for menus with a switch state based on a string or even based on a byte with values predefined?
      if (buttons & BUTTON_UP) {
        //buttonTone();
        previousShot();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_DOWN) {
        //buttonTone();
        nextShot();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_LEFT) {
        ////buttonTone();
        reviewShot--;
        nextShot();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_RIGHT) {
        ////buttonTone();
        rateOfFire();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_SELECT) {
        //buttonTone();
        tm.select();
        DEBUG_PRINTLN(NULL, 0);
      }
    }
    else if (settingDelay == 1) { //setting delay
      if (buttons & BUTTON_UP) {
        //buttonTone();
        increaseDelay();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_DOWN) {
        //buttonTone();
        decreaseDelay();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_LEFT) {
        ////buttonTone();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_RIGHT) {
        ////buttonTone();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_SELECT) {
        //buttonTone();
        tm.select();
        DEBUG_PRINTLN(NULL, 0);
      }
    }
    else if (settingBeep == 1) { //setting beep volume
      if (buttons & BUTTON_UP) {
        //buttonTone();
        increaseBeepVol();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_DOWN) {
        //buttonTone();
        decreaseBeepVol();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_LEFT) {
        ////buttonTone();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_RIGHT) {
        ////buttonTone();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_SELECT) {
        //buttonTone();
        tm.select();
        DEBUG_PRINTLN(NULL, 0);
      }
    }
    else if (settingSensitivity == 1) { //setting sensitivity
      if (buttons & BUTTON_UP) {
        //buttonTone();
        increaseSensitivity();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_DOWN) {
        //buttonTone();
        decreaseSensitivity();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_LEFT) {
        ////buttonTone();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_RIGHT) {
        ////buttonTone();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_SELECT) {
        //buttonTone();
        tm.select();
        DEBUG_PRINTLN(NULL, 0);
      }
    }
    else if (settingEcho == 1) { //setting echo protection
      if (buttons & BUTTON_UP) {
        //buttonTone();
        increaseEchoProtect();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_DOWN) {
        //buttonTone();
        decreaseEchoProtect();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_LEFT) {
        ////buttonTone();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_RIGHT) {
        ////buttonTone();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_SELECT) {
        //buttonTone();
        tm.select();
        DEBUG_PRINTLN(NULL, 0);
      }
    }
    else if (settingParState == 1) { //settingParState
      if (buttons & BUTTON_UP) {
        //buttonTone();
        toggleParState();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_DOWN) {
        //buttonTone();
        toggleParState();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_LEFT) {
        ////buttonTone();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_RIGHT) {
        ////buttonTone();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_SELECT) {
        //buttonTone();
        tm.select();
        DEBUG_PRINTLN(NULL, 0);
      }
    }
    else if (editingPar == 1) { //editing a Par time
      if (buttons & BUTTON_UP) {
        //buttonTone();
        increaseTime();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_DOWN) {
        //buttonTone();
        decreaseTime();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_LEFT) {
        //buttonTone();
        leftCursor();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_RIGHT) {
        //buttonTone();
        rightCursor();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_SELECT) {
        //buttonTone();
        editPar();
        DEBUG_PRINTLN(NULL, 0);
      }
    }
    else if (settingParTimes == 1) { //settingParState
      if (buttons & BUTTON_UP) {
        //buttonTone();
        parUp();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_DOWN) {
        //buttonTone();
        parDown();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_LEFT) {
        //buttonTone();
        tm.select();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_RIGHT) {
        ////buttonTone();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_SELECT) {
        //buttonTone();
        editPar();
        DEBUG_PRINTLN(NULL, 0);
      }
    }
    else  {                     //on the main menu
      if (buttons & BUTTON_UP) {
        //buttonTone();
        tm.prev();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_DOWN) {
        //buttonTone();
        tm.next();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_LEFT) {
        //buttonTone();
        tm.back();
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_RIGHT) {
        //buttonTone();
        tm.select(); //?? How will we make sure to render selected Menus off of the main area, while not allowing it to make MenuItems 'go'? Maybe checking whether the current item is a Menu or MenuItem? Is that possible? 
        DEBUG_PRINTLN(NULL, 0);
      }
      if (buttons & BUTTON_SELECT) {
        //buttonTone();
        tm.select();
        DEBUG_PRINTLN(NULL, 0);
      }
    }
  }
}








