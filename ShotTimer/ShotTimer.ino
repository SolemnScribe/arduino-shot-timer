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
// shotListener(); probably could be redesigned to run on a timer interupt.
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
//#define DEBUG  //comment this out to disable debug information and remove all DEBUG messages at compile time
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
// Other helpful resources
//////////////
// Adafruit sound level sampling: http://learn.adafruit.com/adafruit-microphone-amplifier-breakout/measuring-sound-levels
// http://stackoverflow.com/questions/18903528/permanently-changing-value-of-parameter
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
const uint8_p PROGMEM micPin = A0; //set the input for the mic/amplifier // the mic/amp is connected to analog pin 0
const uint8_p PROGMEM buttonVol = 5;
const uint8_p PROGMEM buttonDur = 80;
const int16_p PROGMEM beepDur = 400;
const int16_p PROGMEM beepNote = NOTE_C4;
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
const char PROGMEM setDelayName[] = "<< [Set Delay] ";
const char PROGMEM buzzerName[] = "<< [Buzzr Vol]";
const char PROGMEM sensitivityName[] = "<< [Sensitivity]";
const char PROGMEM echoName[] = "<< [Echo Reject]";

//////////////
// Instantiation //@TODO: should maybe have a settings object and a timer object? 
//////////////
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

/////////////////////////////////////////
// GLOBAL VARIABLES
/////////////////////////////////////////
byte delayTime = 11;
byte beepVol = 0;
byte sensitivity = 1;
byte sampleWindow = 50;
uint32_t shotTimes[200]; // do we want to instantiate the size in setup()
uint32_t parTimes[10]; // does this HAVE to be 10 for the par setting interface to work? Hardcoded?
uint32_t additivePar;
byte currentShot; // REFACTOR, MAY NOT NEED TO BE GLOBAL
byte reviewShot;  // REFACTOR, MAY NOT NEED TO BE GLOBAL
byte currentPar;  // REFACTOR, MAY NOT NEED TO BE GLOBAL
int threshold = 625; //The sensitivity setting is converted into a threshold value
byte parCursor = 1;

///////////////
// Program State Variables
///////////////
// http://stackoverflow.com/questions/18903528/permanently-changing-value-of-parameter
uint8_t buttonsState;
boolean parEnabled;
enum programState {
  MENU,         // Navigating menus
  TIMER,       // Timer is running // && parEnabled
  REVIEW,       // 2 - Reviewing shots 
  SETPARSTATE,  // 3 - Setting Par State // && parEnabled
  SETPARTIMES,  // 4 - Setting Par Times
  SETINDPAR,    // 5 - ?? Editing Par // Setting Single Par
  SETDELAY,     // 6 - Setting Delay
  SETBEEP,      // 7 - Setting Beep
  SETSENS,      // 8 - Setting Sensitivity 
  SETECHO       // 9 - Setting Echo
 } currentState; 

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
// Note: Any functions with reference parameters i.e myFunction(char &str); must be prototyped manually
// A prototype is simply an empty declaration
//////////////

//////////////////////////////////////////////////////////
// Render the current menu screen
//////////////////////////////////////////////////////////

void renderMenu() {
  Menu const* menu = tm.get_current_menu();
  lcd.setBacklight(WHITE);
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
  uint32_t startMillis = millis(); // Start of sample window --- the peak to peak reading
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
// Start the Shot Timer
//////////////////////////////////////////////////////////
// Consider changing these to be 'on_menu_event()' functions - such that they can have a local variable for whether the menu item is active, rather than using a global. 
//////////////////////////////////////////////////////////

void on_menuStart_selected(MenuItem* p_menu_item) {
  DEBUG_PRINTLN(F("Starting Timer"),1);
  currentState = TIMER;
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
  lcd.print(F(" GO!!  Shot#    ")); //lcd.setCursor(0, 13);
  lcd.setCursor(0, 1);
  lcd.print(F("Last:")); //10 chars
  BEEP();
  shotChrono.restart();
}

//////////////////////////////////////////////////////////
// Run the shot timer - runs in loop()
//////////////////////////////////////////////////////////

void runTimer(programState* pState, boolean* parState) // @TODO: Decide if passing in current state as an argument or just accessing as a global variable!
{
  //DEBUG_PRINTLN(*runState, 0);
  if (*pState == TIMER)
  { 
    //DEBUG_PRINTLN(F("Enter Run Timer Mode."), 0);
    shotListener();
    parBeeps(parState);
  }
}
//////////////////////////////////////////////////////////
// Beep at each par time - runs indirectly in loop()
//////////////////////////////////////////////////////////

void parBeeps(boolean* parState)
{
    //DEBUG_PRINTLN(*parState, 0);
    if (*parState == true) {
      DEBUG_PRINTLN(F("...check for par beep..."),0)
      additivePar = 0;
      for (byte i = 0; i < sizeof(parTimes); i++) {
        if (parTimes[i] == 0) {
          break;
        }
        additivePar += parTimes[i]; // add the parTimes together
        //if (shotTimer.elapsed() <= (additivePar + (sampleWindow / 2)) && shotTimer.elapsed() >= (additivePar - sampleWindow / 2)){
        int timeElapsed = shotChrono.elapsed();
        if (timeElapsed <= (additivePar + (sampleWindow / 2)) && timeElapsed >= (additivePar - sampleWindow / 2)) {
          DEBUG_PRINTLN(F("PAR BEEP!"),0);
          BEEP();  //Beep if the current time matches (within the boundaries of sample window) the parTime
        }
      }

    }
}

//////////////////////////////////////////////////////////
// Stop the shot timer
//////////////////////////////////////////////////////////
void stopTimer(boolean out = 0) {
  DEBUG_PRINTLN(F("Stopping Timer"),0);
  //currentState = REVIEW;
  if (out == 1) {
    lcd.setBacklight(RED);
  }
  else {
    lcd.setBacklight(WHITE);
  }
  //shotTimer.stop();
  DEBUG_PRINTLN(F("Timer was stopped at:"), 0);
  shotChrono.elapsed(); // for DEBUG
  for (int i = 0; i < 5; i++) {
    toneAC(beepNote, beepVol, 100, false); 
    delay(50);
  }
  if (out == 1) {
    lcd.setBacklight(WHITE);
  }
  // Also - transition menus and re-render menu screens in the stop condition. 
  tm.next(); //move the menu down to review mode
  tm.select(); //move into shot review mode immediately
}

//////////////////////////////////////////////////////////
// Record shots to the string array
//////////////////////////////////////////////////////////

void recordShot() {
  //shotTimes[currentShot] = shotTimer.elapsed();
  shotTimes[currentShot] = shotChrono.elapsed();
  DEBUG_PRINT(F("Shot #")); DEBUG_PRINT(currentShot + 1); DEBUG_PRINT(F(" - "));// serialPrintln(shotTimes[currentShot], 9);
  DEBUG_PRINT(F("\n"));
  //serialPrintln(shotTimer.elapsed());
  //serialPrintln(shotChrono.elapsed(), 9);
  lcd.setCursor(13, 0);
  lcdPrint(&lcd, currentShot + 1, 3);
  lcd.setCursor(6, 1);
  lcdPrintTime(&lcd, shotTimes[currentShot], 9); //lcd.print(F(" ")); if(currentShot > 1) {lcdPrintTime(&lcd, shotTimes[currentShot]-shotTimes[currentShot-1],6);}
  //9 characters             //1 characters                    //6 characters
  currentShot += 1;
  if (currentShot == sizeof(shotTimes)) { // if the current shot == 100 (1 more than the length of the array)
    DEBUG_PRINTLN(F("Out of room for shots"),0);
    stopTimer(1);
  }
}

//////////////////////////////////////////////////////////
//review shots - initialize the shot review screen
//////////////////////////////////////////////////////////

void on_menuReview_selected(MenuItem* p_menu_item) {
  if(currentState != REVIEW){
    DEBUG_PRINTLN(F("Enter REVIEW Mode"), 0);
    DEBUG_PRINTLN(currentState, 0);
    currentState = REVIEW;
    DEBUG_PRINTLN(currentState, 0);
    if (currentShot > 0) {
      reviewShot = currentShot - 1;
    }
    DEBUG_PRINT(F("Reviewing Shot: ")); DEBUG_PRINTLN(reviewShot,0);
    //DEBUG FOR LOOP - PRINT ALL SHOT TIMES IN THE STRING TO SERIAL 
    for (int t = 0; t < currentShot; t++) {
      DEBUG_PRINT(F("Shot #"));
      DEBUG_PRINT(t + 1);
      DEBUG_PRINT(F(" - "));
      shotTimes[t]; // for DEBUG
    }
    lcd.setBacklight(VIOLET);
    lcd.setCursor(0, 0);
    lcd.print(F("Shot #"));
    lcd.print(currentShot);
    lcd.print(F("                "));
    lcd.setCursor(9, 0);
    lcd.print(F(" Split "));
    lcd.setCursor(0, 1);
    lcdPrintTime(&lcd, shotTimes[reviewShot], 9);
    lcd.print(F(" "));
    if (reviewShot > 1) {
      lcdPrintTime(&lcd, shotTimes[reviewShot] - shotTimes[reviewShot - 1], 6);
    }
    DEBUG_PRINTLN(tm.get_current_menu()->get_name(),0);
    //9 characters             //1 characters                    //6 characters
  } else {
    DEBUG_PRINTLN(F("Return to Menu"), 0);
    DEBUG_PRINTLN(currentState, 0);
    currentState = MENU;
    renderMenu();
  }
}

//////////////////////////////////////////////////////////
// review shots - move to the next shot in the string
//////////////////////////////////////////////////////////

void nextShot() {
  DEBUG_PRINTLN(F("nextShot()"), 0);
  DEBUG_PRINTLN(currentState, 0);
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
  lcdPrintTime(&lcd, shotTimes[reviewShot], 9);
  lcd.print(F(" "));
  if (reviewShot == 0) {
    lcd.print(F("   1st"));
  }
  else {
    lcdPrintTime(&lcd, shotTimes[reviewShot] - shotTimes[reviewShot - 1], 6);
  }
}

//////////////////////////////////////////////////////////
// review shots - move to the previous shot in the string
//////////////////////////////////////////////////////////

void previousShot() {
  DEBUG_PRINTLN(F("previousShot()"), 0);
  DEBUG_PRINTLN(currentState, 0);
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
  lcdPrintTime(&lcd, shotTimes[reviewShot], 9);
  lcd.print(F(" "));
  if (reviewShot == 0) {
    lcd.print(F("   1st"));
  }
  else {
    lcdPrintTime(&lcd, shotTimes[reviewShot] - shotTimes[reviewShot - 1], 6);
  }
}

//////////////////////////////////////////////////////////
// Rate of Fire
//////////////////////////////////////////////////////////

void rateOfFire(boolean includeDraw = true) {
  DEBUG_PRINTLN(currentState, 0);
  DEBUG_PRINTLN(F("rateofFire()"), 0);
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
  lcdPrintTime(&lcd, rof, 6);
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
  if(currentState != SETDELAY){
    DEBUG_PRINTLN(F("Enter SETDELAY Mode"), 0);
    currentState = SETDELAY;
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
    DEBUG_PRINTLN(F("Save Delay and Return to Menu"), 0);
    delaySetting = delayTime;
    currentState = MENU;
    renderMenu();
  }
}

/////////////////////////////////////////////////////////////
// increaseDelay
/////////////////////////////////////////////////////////////

void increaseDelay() {
  DEBUG_PRINTLN(F("increaseDelay()"), 0);
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
  DEBUG_PRINTLN(F("decreaseDelay()"), 0);
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
  DEBUG_PRINT(F("Start Delay: ")); DEBUG_PRINTLN(delayTime, 0);
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
  if(currentState != SETBEEP){
    DEBUG_PRINTLN(F("Enter SETBEEP Mode"), 0);
    currentState = SETBEEP;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Buzzer Volume"));
    lcd.setCursor(0, 1);
    lcd.print(beepVol);
    //lcdPrint(&lcd, beepVol, 2);
  }
  else {
    DEBUG_PRINTLN(F("Save BeepVol and Return to Menu"), 0);
    beepSetting = beepVol;
    currentState = MENU;
    renderMenu();
  }
}

/////////////////////////////////////////////////////////////
// increaseBeepVol
/////////////////////////////////////////////////////////////

void increaseBeepVol() {
  DEBUG_PRINTLN(F("increaseBeepVoly()"), 0);
  if (beepVol == 10) {
    beepVol = 0;
  }
  else {
    beepVol++;
  }
  lcd.setCursor(0, 1);
  lcd.print(beepVol);
  //lcdPrint(&lcd, beepVol, 2);
  lcd.print(F("                ")); //TODO REplace with a single PROGMEM clear buffer
}

/////////////////////////////////////////////////////////////
// decreaseBeepVol
/////////////////////////////////////////////////////////////

void decreaseBeepVol() {
  DEBUG_PRINTLN(F("decreaseBeepVoly()"), 0);
  if (beepVol == 0) {
    beepVol = 10;
  }
  else {
    beepVol--;
  }
  lcd.setCursor(0, 1);
  lcd.print(beepVol);
  //lcdPrint(&lcd, beepVol, 2);
  lcd.print(F("                "));
}

/////////////////////////////////////////////////////////////
// on_menuSensitivity_selected
/////////////////////////////////////////////////////////////

void on_menuSensitivity_selected(MenuItem* p_menu_item) {
    if(currentState != SETSENS){
    DEBUG_PRINTLN(F("Enter SETSENS Mode"), 0);
    currentState = SETSENS;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Sensitivity"));
    lcd.setCursor(0, 1);
    lcd.print(sensitivity);
    //lcdPrint(&lcd, sensitivity, 2);
  }
  else {
    DEBUG_PRINTLN(F("Save Sensitivity and Return to Menu"), 0);
    sensSetting = sensitivity;
    currentState = MENU;
    renderMenu();
  }
}

/////////////////////////////////////////////////////////////
// increaseSensitivity
/////////////////////////////////////////////////////////////

void increaseSensitivity() {
  DEBUG_PRINTLN(F("increaseSensitivity()"), 0);
  if (sensitivity == 20) {
    sensitivity = 0;
  }
  else {
    sensitivity++;
  }
  sensToThreshold();
  lcd.setCursor(0, 1);
  lcd.print(sensitivity);
  //lcdPrint(&lcd, sensitivity, 2);
  lcd.print(F("                "));
}

/////////////////////////////////////////////////////////////
// decreaseSensitivity
/////////////////////////////////////////////////////////////

void decreaseSensitivity() {
  DEBUG_PRINTLN(F("decreaseSensitivity()"), 0);
  if (sensitivity == 0) {
    sensitivity = 20;
  }
  else {
    sensitivity--;
  }
  sensToThreshold();
  lcd.setCursor(0, 1);
  lcd.print(sensitivity);
  //lcdPrint(&lcd, sensitivity, 2);
  lcd.print(F("                "));
}

/////////////////////////////////////////////////////////////
// on_menuEcho_selected - EEPROM
/////////////////////////////////////////////////////////////

void on_menuEcho_selected(MenuItem* p_menu_item) {
  if(currentState != SETECHO){
    DEBUG_PRINTLN(F("Enter SETECHO Mode"), 0);
    currentState = SETECHO;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Echo Protect"));
    lcd.setCursor(0, 1);
    lcd.print(sampleWindow);
    lcd.print(F("ms"));
  }
  else {
    DEBUG_PRINTLN(F("Save Echo and Return to Menu"), 0);
    sampleSetting = sampleWindow;
    currentState = MENU;
    renderMenu();
  }
}

/////////////////////////////////////////////////////////////
// increaseEchoProtect
/////////////////////////////////////////////////////////////

void increaseEchoProtect() {
  DEBUG_PRINTLN(F("increaseEchoProtect()"), 0);
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
  DEBUG_PRINTLN(F("decreaseEchoProtect()"), 0);
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
  DEBUG_PRINT(F("State before select: ")); DEBUG_PRINTLN(currentState,0);
  DEBUG_PRINTLN(tm.get_current_menu()->get_name(),0);
  if(currentState != SETPARSTATE){
    DEBUG_PRINTLN(F("Enter SETPARSTATE Mode"),0);
    currentState = SETPARSTATE;
    DEBUG_PRINT(F("State after select: ")); DEBUG_PRINTLN(currentState,0);
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
    DEBUG_PRINTLN(F("Return to Menu"), 0);
    currentState = MENU;
    DEBUG_PRINT(F("State after select: ")); DEBUG_PRINTLN(currentState,0);
    renderMenu();
  }
}

/////////////////////////////////////////////////////////////
// toggleParState
/////////////////////////////////////////////////////////////

void toggleParState() {
  parEnabled = !parEnabled;
  DEBUG_PRINT(F("currentState: ")); DEBUG_PRINTLN(currentState,0);
  DEBUG_PRINT(F("Toggled Par to: "));DEBUG_PRINTLN(parEnabled, 0);
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
    DEBUG_PRINTLN_P(tm.get_current_menu()->get_selected()->get_name(),0);
    DEBUG_PRINT(F("State before select: ")); DEBUG_PRINTLN(currentState,0);
  if(currentState != SETPARTIMES){
    DEBUG_PRINTLN(F("Enter SETPARTIMES Mode"), 0);
    currentState = SETPARTIMES;
    DEBUG_PRINT(F("State after select: ")); DEBUG_PRINTLN(currentState,0);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("<<"));
    lcd.setCursor(5, 0);
    lcd.print(F("Par"));
    lcd.setCursor(9, 0);
    //lcd.print(currentPar + 1);
    lcdPrint(&lcd, (currentPar + 1), 2);
    lcd.setCursor(4, 1);
    if (currentPar > 0) {
      lcd.print(F("+"));
    }
    else {
      lcd.print(F(" "));
    }
    //lcd.print(parTimes[currentPar]);
    lcdPrintTime(&lcd, parTimes[currentPar], 9);
    DEBUG_PRINTLN_P(tm.get_current_menu()->get_selected()->get_name(),0);
  }
  else {
    currentState = MENU;
    DEBUG_PRINT(F("State after select: ")); DEBUG_PRINTLN(currentState,0);
    renderMenu();
  }
}

/////////////////////////////////////////////////////////////
// parUp()
/////////////////////////////////////////////////////////////

void parUp() {
  DEBUG_PRINTLN(F("parUp()"), 0);
  if (currentPar == 0) {
    currentPar = sizeof(parTimes) - 1;
  }
  else {
    currentPar--;
  }
  lcd.setCursor(9, 0);
  //lcd.print(currentPar + 1); lcd.print(" ");
  lcdPrint(&lcd, (currentPar + 1), 2);
  lcd.setCursor(4, 1);
  if (currentPar > 0) {
    lcd.print(F("+"));
  }
  else {
    lcd.print(F(" "));
  }
  lcdPrintTime(&lcd, parTimes[currentPar], 9);
  DEBUG_PRINTLN_P(tm.get_current_menu()->get_selected()->get_name(),0);
}

/////////////////////////////////////////////////////////////
// parDown()
/////////////////////////////////////////////////////////////

void parDown() {
  DEBUG_PRINTLN(F("parDown()"), 1);
  if (currentPar == sizeof(parTimes) - 1) {
    currentPar = 0;
  }
  else {
    currentPar++;
  }
  lcd.setCursor(9, 0);
  //lcd.print(currentPar + 1);
  lcdPrint(&lcd, (currentPar + 1), 2);
  lcd.setCursor(4, 1);
  if (currentPar > 0) {
    lcd.print(F("+"));
  }
  else {
    lcd.print(F(" "));
  }
  //DEBUG_PRINTLN(F("FreeRam()"), 1);
  lcdPrintTime(&lcd, parTimes[currentPar], 9);
  DEBUG_PRINTLN_P(tm.get_current_menu()->get_selected()->get_name(),0);
}

/////////////////////////////////////////////////////////////
// editPar()
/////////////////////////////////////////////////////////////

void editPar() {
  if(currentState != SETINDPAR){
    DEBUG_PRINTLN(F("Enter SETINDPAR Mode"), 0);
    currentState = SETINDPAR;
    lcd.setBacklight(GREEN);
    lcd.setCursor(0, 0);
    lcd.print(F("Edit        "));
    lcd.setCursor(0, 1);
    lcd.print(F("P"));
    //lcd.print(currentPar + 1);
    lcdPrint(&lcd, currentPar + 1, 2);
    if (currentPar > 0) {
      lcd.print(F(" +"));
    }
    else {
      lcd.print(F("  "));
    }
    lcd.setCursor(5, 1);
    lcdPrintTime(&lcd, parTimes[currentPar], 9);
    parCursor = 4; //reset cursor to the seconds position
    lcdCursor();
  }
  else {
    DEBUG_PRINTLN(F("Return to SETPARTIMES"), 0);
    Serial.print(currentState);
    DEBUG_PRINTLN_P(tm.get_current_menu()->get_selected()->get_name(),0);
    lcd.setBacklight(WHITE);
    //on_menuParTimes_selected(&menuParTimes);
    tm.select();
  }
}

/////////////////////////////////////////////////////////////
// leftCursor()
/////////////////////////////////////////////////////////////

void leftCursor() {
  DEBUG_PRINTLN(F("leftCursor()"), 0);
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
  DEBUG_PRINTLN(F("leftCursor()"), 0);
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
  DEBUG_PRINT(F("Displaying Cursor at: "));DEBUG_PRINTLN(parCursor, 0);
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
  DEBUG_PRINT(F("Increase time at: "));DEBUG_PRINTLN(parCursor, 0);
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
  lcdPrintTime(&lcd, parTimes[currentPar], 9);
}

/////////////////////////////////////////////////////////////
// decreaseTime()
/////////////////////////////////////////////////////////////

void decreaseTime() {
  DEBUG_PRINT(F("Decrease time at: "));DEBUG_PRINTLN(parCursor, 0);
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
  lcdPrintTime(&lcd, parTimes[currentPar], 9);
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
// Note - EEWrap automatically uses an .update() on EEPROM writes, 
// to avoid wearing out the EEPROM if the value being set is the same as the existing value. 
/////////////////////////////////////////////////////////////

void eepromSetup() {
  DEBUG_PRINTLN(F("Checking if EEPROM needs to be set..."), 0);

  // Use settings values from EEPROM only if the if non-null values have been set
  // Because 255 is the default for unused EEPROM and not a valid value for Sample Window...
  // if ANY of our EEPROM stored settings come back 255, we'll know that the EEPROM settings have not been set
  // By checking all 4 settings, we help ensure that legacy EEPROM data doesn't slip in and cause unexpected behavior.
  byte unSet = 255;
  
  if (sampleSetting == unSet || sensSetting == unSet || beepSetting == unSet || delaySetting == unSet) {
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
    DEBUG_PRINTLN(F("Reading settings from EEPROM"), 0);
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
  DEBUG_PRINTLN(F("Setting up the LCD"),0);
  lcd.begin(16, 2);
  lcd.setBacklight(WHITE);
  renderMenu();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Listeners
////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////
// Button Listener
// returns true if the button state 
//////////////
void buttonListener(Adafruit_RGBLCDShield* lcd, uint8_t* bState, programState* pState) {
  //DEBUG_PRINT(F("pState: ")); DEBUG_PRINTLN(*pState,0);
  //DEBUG_PRINT(F("currentState: ")); DEBUG_PRINTLN(currentState,0);
  /////////////////////////////
  // buttonStateManager
  /////////////////////////////
  //DEBUG_PRINTLN(F("Listening to button input"),0);
  uint8_t stateNow = lcd->readButtons();
  //DEBUG_PRINT(F("stateNow: ")); DEBUG_PRINTLN(stateNow,0);
  //DEBUG_PRINT(F("bState: ")); DEBUG_PRINTLN(*bState, 0);
  uint8_t newButton = stateNow & ~*bState; // if the current and the last state are different this is true
  if (newButton) {DEBUG_PRINT(F("Button Pushed: "));}
  *bState = stateNow;
  /////////////////////////////

  /////////////////////////////
  // buttonReactor
  /////////////////////////////
  //PSEUDOCODE
  // buttonListener listens for input
  // check(currentState) 
  // reactToButtons(currentState); <-- simplify to just reactToButtons?
  // while reacting to buttons
  //  // switch currentState
  //  // change what buttons do per state
  //  // OR
  //  // For each button
  //  // decide what to do by default
  //  // override if it changes based on currentState
  // SWITCH CASES ARE BETTER FOR MAINTAINABILITY, BUT LIKELY SLOWER
  // http://blog.kriegsman.org/2013/12/01/optimizing-10-lines-of-arduino-code-youre-wrong-so-am-i/
  // This may be important - ESPECIALLY for the case of making sure the TIMER runs efficiently. 
  // In fact - may pull that program state into an if, and put all the rest of the cases into the SWITCH. 
  // @TODO: MenuSystem doesn't loop? 
  
  switch (*pState){
    case MENU:
      switch (newButton) {
        case BUTTON_SELECT:
          //const Menu* menu2 = tm.get_current_menu();
          //Serial.println(tm.get_current_menu()->get_name());
          DEBUG_PRINTLN(F("SELECT/SELECT"), 0);
          DEBUG_PRINTLN_P(tm.get_current_menu()->get_name(),0);
          //DEBUG_PRINTLN_P(menu2->get_name(),0);
          //DEBUG_PRINTLN_P(menu2->get_selected()->get_name(),0);
          tm.select();
          if(currentState == MENU){renderMenu();}
          break;
        case BUTTON_RIGHT:
          //Menu const* menu = tm.get_current_menu();
          DEBUG_PRINTLN(F("RIGHT/SELECT"), 0);
          //DEBUG_PRINTLN(menu->get_name(),0);
          tm.select();
          if(currentState == MENU){renderMenu();}
          break;
        case BUTTON_LEFT:
          DEBUG_PRINTLN(F("LEFT/BACK"), 0);
          tm.back();
          renderMenu();
          break;
        case BUTTON_DOWN:
          DEBUG_PRINTLN(F("DOWN/NEXT"), 0);
          tm.next();
          renderMenu();
          break;
        case BUTTON_UP:
          DEBUG_PRINTLN(F("UP/PREV"), 0);
          tm.prev();
          renderMenu();
          break;
        }
      break;
    case TIMER:
      if (newButton & BUTTON_SELECT){
        DEBUG_PRINTLN(F("SELECT/stopTimer()"), 0);
        stopTimer();
      }
      break;
    case REVIEW:
      switch (newButton) {
        case BUTTON_SELECT:
          DEBUG_PRINTLN(F("SELECT/SELECT"), 0);
          tm.select();
          break;
        case BUTTON_RIGHT:
          DEBUG_PRINTLN(F("RIGHT/rateOfFire()"), 0);
          rateOfFire();
          break;
        case BUTTON_LEFT:
          DEBUG_PRINTLN(F("LEFT/reviewShot--;nextShot()"), 0);
          reviewShot--;
          nextShot();
          break;
        case BUTTON_DOWN:
          DEBUG_PRINTLN(F("DOWN/nextShot()"), 0);
          nextShot(); //@TODO<-- Maybe I should be building a shot string class, with functions, rather than using functions to operate on a global array. 
          break;
        case BUTTON_UP:
          DEBUG_PRINTLN(F("UP/previousShot()"), 0);
          previousShot();
          break;
        }
      break;
    case SETPARSTATE:
      switch (newButton) {
        case BUTTON_SELECT:
          DEBUG_PRINTLN(F("SELECT/(BACK)SELECT"), 0);
          DEBUG_PRINTLN_P(tm.get_current_menu()->get_name(),0);
          tm.select();
          DEBUG_PRINTLN_P(tm.get_current_menu()->get_name(),0);
          break;
        case BUTTON_LEFT:
          DEBUG_PRINTLN(F("LEFT/(BACK)SELECT"), 0);
          tm.select();
          break;
        case BUTTON_DOWN:
          DEBUG_PRINTLN(F("DOWN/toggleParState()"), 0);
          toggleParState(); //@TODO<-- Maybe I should build a par times class with par state and array of part times - and have functions on the class 
                            // if function names of all objects I manipulate with my buttons are the same and on the same buttons - could use polymorphism? 
          break;
        case BUTTON_UP:
          DEBUG_PRINTLN(F("UP/toggleParState()"), 0);
          toggleParState();
          break;
        }
      break;
    case SETPARTIMES:
      switch (newButton) {
        case BUTTON_SELECT:
          DEBUG_PRINTLN_P(tm.get_current_menu()->get_selected()->get_name(),0);
          DEBUG_PRINTLN(F("SELECT/editPar"), 0);
          editPar();
          break;
        case BUTTON_LEFT:
          DEBUG_PRINTLN(F("LEFT/(BACK)SELECT"), 0);
          DEBUG_PRINTLN_P(tm.get_current_menu()->get_selected()->get_name(),0);
          tm.select(); //@TODO: Bug here
          break;
        case BUTTON_DOWN:
          DEBUG_PRINTLN(F("DOWN/parDown()"), 0);
          DEBUG_PRINTLN_P(tm.get_current_menu()->get_selected()->get_name(),0);
          parDown(); //@TODO<-- Maybe I should be building a shot string class, with functions, rather than using functions to operate on a global array. 
          break;
        case BUTTON_UP:
          DEBUG_PRINTLN(F("UP/parUp()"), 0);
          DEBUG_PRINTLN_P(tm.get_current_menu()->get_selected()->get_name(),0);
          parUp();
          break;
        }
      break;
    case SETINDPAR:
      switch (newButton) {
        case BUTTON_SELECT:
          DEBUG_PRINTLN(F("SELECT/editPar()"), 0);
          editPar();
          break;
        case BUTTON_RIGHT:
          DEBUG_PRINTLN(F("RIGHT/rightCursor()"), 0);
          rightCursor();
          break;
        case BUTTON_LEFT:
          DEBUG_PRINTLN(F("LEFT/leftCursor()"), 0);
          leftCursor();
          break;
        case BUTTON_DOWN:
          DEBUG_PRINTLN(F("DOWN/decreaseTime()"), 0);
          decreaseTime();
          break;
        case BUTTON_UP:
          DEBUG_PRINTLN(F("UP/increaseTime()"), 0);
          increaseTime();
          break;
        }
      break;
    case SETDELAY:
      switch (newButton) {
        case BUTTON_SELECT:
          DEBUG_PRINTLN(F("SELECT/SELECT"), 0);
          tm.select();
          break;
        case BUTTON_LEFT:
          DEBUG_PRINTLN(F("LEFT/(BACK)SELECT"), 0);
          tm.select(); //@TODO: Bug here
          break;
        case BUTTON_DOWN:
          DEBUG_PRINTLN(F("DOWN/decreaseDelay()"), 0);
          decreaseDelay(); //@TODO<-- Maybe I should be building a shot string class, with functions, rather than using functions to operate on a global array. 
          break;
        case BUTTON_UP:
          DEBUG_PRINTLN(F("UP/increaseDelay()"), 0);
          increaseDelay();
          break;
        }
      break;
    case SETBEEP:
      switch (newButton) {
        case BUTTON_SELECT:
          DEBUG_PRINTLN(F("SELECT/SELECT"), 0);
          tm.select();
          break;
        case BUTTON_LEFT:
          DEBUG_PRINTLN(F("LEFT/(BACK)SELECT"), 0);
          tm.select(); //@TODO: Bug here
          break;
        case BUTTON_DOWN:
          DEBUG_PRINTLN(F("DOWN/decreaseBeepVol()"), 0);
          increaseBeepVol();
          break;
        case BUTTON_UP:
          DEBUG_PRINTLN(F("UP/increaseBeepVol()"), 0);
          decreaseBeepVol();
          break;
        }
      break;
    case SETSENS:
      switch (newButton) {
        case BUTTON_SELECT:
          DEBUG_PRINTLN(F("SELECT/SELECT"), 0);
          tm.select();
          break;
        case BUTTON_LEFT:
          DEBUG_PRINTLN(F("LEFT/(BACK)SELECT"), 0);
          tm.select(); //@TODO: Bug here
          break;
        case BUTTON_DOWN:
          DEBUG_PRINTLN(F("DOWN/decreaseSensitivity())"), 0);
          decreaseSensitivity();
          break;
        case BUTTON_UP:
          DEBUG_PRINTLN(F("UP/increaseSensitivity()"), 0);
          increaseSensitivity();
          break;
        }
      break;
    case SETECHO:
      switch (newButton) {
        case BUTTON_SELECT:
          DEBUG_PRINTLN(F("SELECT/SELECT"), 0);
          tm.select();
          break;
        case BUTTON_LEFT:
          DEBUG_PRINTLN(F("LEFT/(BACK)SELECT"), 0);
          tm.select(); //@TODO: Bug here
          break;
        case BUTTON_DOWN:
          DEBUG_PRINTLN(F("DOWN/decreaseEchoProtect();)"), 0);
          decreaseEchoProtect();
          break;
        case BUTTON_UP:
          DEBUG_PRINTLN(F("UP/increaseEchoProtect();"), 0);
          increaseEchoProtect();
          break;
        }
      break;
  }
}

//////////////////////////////////////////////////////////
// Listen for Shots
//////////////////////////////////////////////////////////

void shotListener() {
  //DEBUG_PRINTLN(F("Listen-start:"),0);
  if (sampleSound() >= threshold) {
    recordShot();
  }
  //DEBUG_PRINTLN(F("Listen-end:"),0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//SETUP AND LOOP
////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////
// SETUP
//////////////

void setup() {
  randomSeed(analogRead(1));
  DEBUG_SETUP();
  //eepromSetup();
  menuSetup();
  lcdSetup();
  DEBUG_PRINTLN(F("Setup Complete"), 0);
}

//////////////
// LOOP
//////////////

void loop() {
  buttonListener(&lcd, &buttonsState, &currentState); //Possibly all button actions should come before runTimer()
  runTimer(&currentState, &parEnabled); 
} 
