/////////////////////////////////
// Shot Timer
// Author: hestenet
// Canonical Repository: https://github.com/hestenet/arduino-shot-timer
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

//////////////
// INCLUDES
//////////////

//Tones for buttons and buzzer
#include "pitches.h" // musical pitches - optional - format: NOTE_C4 | This include makes no difference to program or dynamic memory

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

//MenuSystem
#include <MenuSystem.h>

//Adafruit RGB LCD Shield Library
//#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>


//////////////
// Libraries - Other
// These are libraries that cannot be found in the defauilt Arduino library manager - however, they can be added manually.
// A source url for each library is provided - simply download the library and include in:
// ~/Documents/Arduino/Libraries
//////////////

//toneAC
//Bit-Bang tone library for piezo buzzer http://code.google.com/p/arduino-tone-ac/
#include <toneAC.h>

//MenuBackend
//A menu system for Arduino https://github.com/WiringProject/Wiring/tree/master/framework/libraries/MenuBackend
//#include <MenuBackend.h> //Documentation: http://wiring.org.co/reference/libraries/MenuBackend/index.html


//#include <StopWatch.h> //http://playground.arduino.cc/Code/StopWatchClass

//////////////
// Other code samples used:
// Adafruit sound level sampling: http://learn.adafruit.com/adafruit-microphone-amplifier-breakout/measuring-sound-levels
//////////////

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
// CONSTANTS
//////////////
//////////////
// PROGMEM
//////////////

//Menu Names - format: const char Name[] PROGMEM = ""; <---- this doesn't work with menuBackEnd and I don't know why.
//To read these - increment over the array - https://github.com/Chris--A/PGMWrap/blob/master/examples/advanced/use_within_classes/use_within_classes.ino 
//More detailed example of dealing with strings and arrays in PROGMEM http://www.gammon.com.au/progmem
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

//MenuBackend timerMenu = MenuBackend(menuUseEvent,menuChangeEvent);
MenuSystem tm;
Menu mainMenu("");
//Menu Items under mainMenu
  //MenuItem menuStart   = MenuItem(timerMenu, startName, 1);
  MenuItem menuStart(startName);
  //MenuItem menuReview    = MenuItem(timerMenu, reviewName, 1);
  MenuItem menuReview(reviewName);

  //MenuItem menuPar    = MenuItem(timerMenu, parName, 1);
  Menu parMenu(parName);

    //MenuItem menuParState = MenuItem(timerMenu, parSetName, 2);
    MenuItem menuParState(parSetName);
    //MenuItem menuParTimes = MenuItem(timerMenu, parTimesName, 2);
    MenuItem menuParTimes(parTimesName);

  //MenuItem menuSettings = MenuItem(timerMenu, settingsName, 1);
  Menu settingsMenu(settingsName);
    //MenuItem menuStartDelay = MenuItem(timerMenu, setDelayName, 2);
    MenuItem menuStartDelay(setDelayName);
    //MenuItem menuBuzzer = MenuItem(timerMenu, buzzerName, 2);
    MenuItem menuBuzzer(buzzerName);
    //MenuItem menuSensitivity = MenuItem(timerMenu, sensitivityName, 2);
    MenuItem menuSensitivity(sensitivityName);
    //MenuItem menuEcho = MenuItem(timerMenu, echoName, 2);
    MenuItem menuEcho(echoName);


//////////////
// FUNCTIONS
//////////////

/////////////////////////////////////////////////////////////
// MENU STATE CHANGES
/////////////////////////////////////////////////////////////

/*
  This is an important function
  Here we get a notification whenever the user changes the menu
  That is, when the menu is navigated
*/
//menuBackend menuChangeEvent() 
//void menuChangeEvent(MenuChangeEvent changed)
//{
//  Serial.print(F("Menu change from "));
//  serialPrintln_p(changed.from.getName());
//  Serial.print(F(" to "));
//  serialPrintln_p(changed.to.getName()); //changed.to.getName()
//  lcd.setCursor(0, 1);
//  lcdPrint_p(changed.to.getName());
//  lcd.print(F("            ")); //12 spaces
//}





//////////////////////////////////////////////////////////
// Return to the menu screen
//////////////////////////////////////////////////////////

void returnToMenu() {
  Menu const* menu;
  menu = tm.get_current_menu();
  //const char* menu1_name = menu->get_selected()->get_name();
  //const char* menu_name = menu->get_selected()->get_name();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Shot Timer v.3"));
  lcd.setCursor(0, 1);
  lcdPrint_p(menu->get_name()); //get_current_menu_name() is apparently no longer a thing! Argh.
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
  //serialPrint(shotTimer.elapsed(), 7);
  serialPrint(shotChrono.elapsed(), 7);
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
  Serial.println(F("Timer was stopped at:"));
  //serialPrint(shotTimer.elapsed(), 7);
  serialPrint(shotChrono.elapsed(), 7);
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
  //Serial.print(F("Shot #")); Serial.print(currentShot + 1); Serial.print(F(" - ")); serialPrint(shotTimes[currentShot], 7);
  //Serial.println(shotTimer.elapsed());
  //Serial.println(shotChrono.elapsed(), 7);
  lcd.setCursor(6, 1);
  lcdPrintTime(shotTimes[currentShot], 7); //lcd.print(F(" ")); if(currentShot > 1) {lcdPrintTime(shotTimes[currentShot]-shotTimes[currentShot-1],5);}
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
    Serial.print(reviewShot);
    for (int t = 0; t < currentShot; t++) {
      Serial.print(F("Shot #"));
      Serial.print(t + 1);
      Serial.print(F(" - "));
      serialPrint(shotTimes[t], 7);
    }
    lcd.setBacklight(VIOLET);
    lcd.setCursor(0, 0);
    lcd.print(F("Shot #"));
    lcd.print(currentShot);
    lcd.print(F("                "));
    lcd.setCursor(9, 0);
    lcd.print(F(" Split "));
    lcd.setCursor(0, 1);
    lcdPrintTime(shotTimes[reviewShot], 7);
    lcd.print(F(" "));
    if (reviewShot > 1) {
      lcdPrintTime(shotTimes[reviewShot] - shotTimes[reviewShot - 1], 5);
    }
    //9 characters             //1 characters                    //6 characters
  }
  else {
    lcd.setBacklight(WHITE);
    returnToMenu();
  }
  Serial.println(reviewingShots);
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
  lcdPrintTime(shotTimes[reviewShot], 7);
  lcd.print(F(" "));
  if (reviewShot == 0) {
    lcd.print(F("   1st"));
  }
  else {
    lcdPrintTime(shotTimes[reviewShot] - shotTimes[reviewShot - 1], 5);
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
  lcdPrintTime(shotTimes[reviewShot], 7);
  lcd.print(F(" "));
  if (reviewShot == 0) {
    lcd.print(F("   1st"));
  }
  else {
    lcdPrintTime(shotTimes[reviewShot] - shotTimes[reviewShot - 1], 5);
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
  lcdPrintTime(rof, 5);
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
    returnToMenu();
  }
  Serial.println(settingDelay);
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
    lcd2digits(beepVol);
  }
  else {
    beepSetting = beepVol;
    returnToMenu();
  }
  Serial.println(settingBeep);
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
  lcd2digits(beepVol);
  lcd.print(F("                "));
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
  lcd2digits(beepVol);
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
    lcd2digits(sensitivity);
  }
  else {
    sensSetting = sensitivity;
    returnToMenu();
  }
  Serial.println(settingSensitivity);
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
  lcd2digits(sensitivity);
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
  lcd2digits(sensitivity);
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
    returnToMenu();
  }
  Serial.println(settingEcho);
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
  lcd.print(F("ms"));
  lcd.print(F("                "));
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
  lcd.print(F("ms"));
  lcd.print(F("                "));
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
    returnToMenu();
  }
  Serial.println(settingParState);
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
    lcd2digits(currentPar + 1);
    lcd.setCursor(4, 1);
    if (currentPar > 0) {
      lcd.print(F("+"));
    }
    else {
      lcd.print(F(" "));
    }
    lcdPrintTime(parTimes[currentPar], 7);
  }
  else {
    returnToMenu();
  }
  Serial.println(settingParState);
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
  lcd2digits(currentPar + 1);
  lcd.setCursor(4, 1);
  if (currentPar > 0) {
    lcd.print(F("+"));
  }
  else {
    lcd.print(F(" "));
  }
  lcdPrintTime(parTimes[currentPar], 7);
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
  lcd2digits(currentPar + 1);
  lcd.setCursor(4, 1);
  if (currentPar > 0) {
    lcd.print(F("+"));
  }
  else {
    lcd.print(F(" "));
  }
  lcdPrintTime(parTimes[currentPar], 7);
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
    lcd2digits(currentPar + 1);
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
  Serial.println(editingPar);
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
  lcdPrintTime(parTimes[currentPar], 7);
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
  lcdPrintTime(parTimes[currentPar], 7);
}

/////////////////////////////////////////////////////////////
// Print time to a serial monitor
/////////////////////////////////////////////////////////////

void serialPrint(uint32_t t, byte digits) //formerly called print2digits: http://arduino.cc/forum/index.php?topic=64024.30
{
  uint32_t x;
  if (digits >= 9)
  {
    // HOURS
    x = t / 3600000UL;
    t -= x * 3600000UL;
    serial2digits(x);
    Serial.print(':');
  }

  // MINUTES
  x = t / 60000UL;
  t -= x * 60000UL;
  serial2digits(x);
  Serial.print(':');

  // SECONDS
  x = t / 1000UL;
  t -= x * 1000UL;
  serial2digits(x);

  if (digits >= 9 || digits >= 7)
  {
    // HUNDREDS/THOUSANDTHS
    Serial.print('.');
    //x = (t+5)/10L;  // rounded hundredths?
    serial3digits(t);
  }
  Serial.print(F("\n"));
}

/////////////////////////////////////////////////////////////
// Serial Helper - 2 digits
/////////////////////////////////////////////////////////////

void serial2digits(uint32_t x)
{
  if (x < 10) Serial.print(F("0"));
  Serial.print(x);
}

/////////////////////////////////////////////////////////////
// Serial Helper - 3 digits
/////////////////////////////////////////////////////////////

void serial3digits(uint32_t x)
{
  if (x < 100) Serial.print(F("0"));
  if (x < 10) Serial.print(F("0"));
  Serial.print(x);
}

/////////////////////////////////////////////////////////////
// Serial Helper - 4 digits
/////////////////////////////////////////////////////////////

void serial4digits(uint32_t x) {
  if (x < 1000) Serial.print(F("0"));
  if (x < 100) Serial.print(F("0"));
  if (x < 10) Serial.print(F("0"));
  Serial.print(x);
}

/////////////////////////////////////////////////////////////
// PROGMEM Helper - Print a string from PROGMEM to an LCD Screen
/////////////////////////////////////////////////////////////

void serialPrintln_p(const char * str)
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
  Serial.print(F("\n"));
}

/////////////////////////////////////////////////////////////
// PROGMEM Helper - Print a string from PROGMEM to an LCD Screen
/////////////////////////////////////////////////////////////

void lcdPrint_p(const char * str)
{
  char c;
  if (!str)
  {
    return;
  }
  while (c = pgm_read_byte(str++))
    lcd.print(c);
}

/////////////////////////////////////////////////////////////
// Print time to an LCD screen
/////////////////////////////////////////////////////////////

void lcdPrintTime(uint32_t t, byte digits)
{
  uint32_t x;
  if (digits >= 8)
  {
    // HOURS
    x = t / 3600000UL;
    t -= x * 3600000UL;
    lcd2digits(x);
    lcd.print(':');
  }
  if (digits >= 6) {
    // MINUTES
    x = t / 60000UL;
    t -= x * 60000UL;
    lcd2digits(x);
    lcd.print(':');
  }
  if (digits >= 4) {
    // SECONDS
    x = t / 1000UL;
    t -= x * 1000UL;
    lcd2digits(x);
  }
  if (digits == 9 || digits == 7 || digits == 5)
  {
    // THOUSANDTHS
    lcd.print('.');
    //x = (t+5)/10L;  // rounded hundredths?
    lcd3digits(t);
  }
  else if (digits >= 2) {
    // ROUNDED HUNDREDTHS
    lcd.print('.');
    x = (t + 5) / 10L; // rounded hundredths?
    lcd2digits(t);
  }
}

/////////////////////////////////////////////////////////////
// LCD Helper - 2 digits
/////////////////////////////////////////////////////////////

void lcd2digits(uint32_t x)
{
  if (x < 10) lcd.print(F("0"));
  lcd.print(x);
}

/////////////////////////////////////////////////////////////
// LCD Helper - 3 digits
/////////////////////////////////////////////////////////////
void lcd3digits(uint32_t x)
{
  if (x < 100) lcd.print(F("0"));
  if (x < 10) lcd.print(F("0"));
  lcd.print(x);
}

/////////////////////////////////////////////////////////////
// LCD Helper - 4 digits
/////////////////////////////////////////////////////////////

void lcd4digits(uint32_t x) {
  if (x < 1000) lcd.print(F("0"));
  if (x < 100) lcd.print(F("0"));
  if (x < 10) lcd.print(F("0"));
  lcd.print(x);
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

//////////////////////////////////////////////////////////
// Menu Setup
//////////////////////////////////////////////////////////

void menuSetup()
{
  mainMenu.add_item(&menuStart, &on_menuStart_selected);
  mainMenu.add_item(&menuReview, &on_menuReview_selected);
  mainMenu.add_menu(&parMenu);
    parMenu.add_item(&menuParState, &on_menuParState_selected);
    parMenu.add_item(&menuParTimes, &on_menuParTimes_selected);
  mainMenu.add_menu(&settingsMenu);
    settingsMenu.add_item(&menuStartDelay, &on_menuStartDelay_selected);
    settingsMenu.add_item(&menuBuzzer, &on_menuBuzzer_selected);
    settingsMenu.add_item(&menuSensitivity, &on_menuSensitivity_selected);
    settingsMenu.add_item(&menuEcho, &on_menuEcho_selected);
  tm.set_root_menu(&mainMenu); 
}

//////////////////////////////////////////////////////////
// LCD Setup
//////////////////////////////////////////////////////////

void lcdSetup() {
  lcd.begin(16, 2);
  // Print a message to the LCD. We track how long it takes since
  // this library has been optimized a bit and we're proud of it :)
  int time = millis();
  lcd.print(F("Shot Timer v.3"));
  lcd.setCursor(0, 1);
  lcd.print(F("[Start]         "));
  time = millis() - time;
  Serial.print(F("Took "));
  Serial.print(time);
  Serial.println(F(" ms"));
  lcd.setBacklight(WHITE);
}

/////////////////////////////////////////////////////////////
// eepromSetup
/////////////////////////////////////////////////////////////

void eepromSetup() {
  Serial.print(F("Checking if EEPROM needs to be set..."));
// Note - EEWrap automatically uses an .update() on EEPROM writes, to avoid wearing out the EEPROM if the value being set is the same as the existing value. 

  // Use settings values from EEPROM only if the if non-null values have been set
  // Because 0 is not a valid value for Sample Window - this is the only variable we need to check for a null value to know that EEPROM is not yet set. When it is not set, set it with default values 
  if (sampleWindow == 0) {
    Serial.println(F("Setting EEPROM"));
    delaySetting = delayTime;
      Serial.println(F("Set delaySetting to "));
      Serial.print(delayTime);
    beepVol = 10;
      Serial.println(F("Set beepSetting to "));
      Serial.print(beepVol);
    sensitivity = 1;
      Serial.println(F("Set sensSetting to "));
      Serial.print(sensitivity);
    sampleWindow = 50;
      Serial.println(F("Set sampleSetting to "));
      Serial.print(sampleWindow);
  }
  else {
    Serial.println(F("Reading settings from EEPROM)"));
    delayTime = delaySetting;
      Serial.println(F("Set delayTime to "));
      Serial.print(delayTime);
    beepVol = beepSetting;
      Serial.println(F("Set beepVol to "));
      Serial.print(beepVol);
    sensitivity = sensSetting;
      Serial.println(F("Set sensitivity to "));
      Serial.print(sensitivity);
    sampleWindow = sampleSetting;
      Serial.println(F("Set sampleWindow to "));
      Serial.print(sampleWindow);
  }
  sensToThreshold(); //make sure that the Threshold is calculated based on the stored sensitivity setting
}


////////////////////////////////////////////////////////////////////
////Checking Free RAM http://playground.arduino.cc/Code/AvailableMemory
////////////////////////////////////////////////////////////////////


// this function will print the free RAM in bytes to the serial port
void freeRAM () {
  extern int __heap_start, *__brkval;
  int v;
  v = (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
  Serial.print(v);
  Serial.print(F("\n"));
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////
//SETUP AND LOOP
////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////
// SETUP
//////////////

void setup() {
  /////////////
  // Debugging:
  /////////////

  //Open serial communications and wait for port to open:
  Serial.begin(9600);
  //    while (!Serial) {
  //      ; // wait for serial port to connect. Needed for Leonardo only
  //    }

  randomSeed(analogRead(1));

  eepromSetup();

  //menuSetup(timerMenu);
  menuSetup();

  lcdSetup();

  freeRAM();
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
        freeRAM();
      }
      if (buttons & BUTTON_DOWN) {
        //buttonTone();
        nextShot();
        freeRAM();
      }
      if (buttons & BUTTON_LEFT) {
        ////buttonTone();
        reviewShot--;
        nextShot();
        freeRAM();
      }
      if (buttons & BUTTON_RIGHT) {
        ////buttonTone();
        rateOfFire();
        freeRAM();
      }
      if (buttons & BUTTON_SELECT) {
        //buttonTone();
        tm.select();
        freeRAM();
      }
    }
    else if (settingDelay == 1) { //setting delay
      if (buttons & BUTTON_UP) {
        //buttonTone();
        increaseDelay();
        freeRAM();
      }
      if (buttons & BUTTON_DOWN) {
        //buttonTone();
        decreaseDelay();
        freeRAM();
      }
      if (buttons & BUTTON_LEFT) {
        ////buttonTone();
        freeRAM();
      }
      if (buttons & BUTTON_RIGHT) {
        ////buttonTone();
        freeRAM();
      }
      if (buttons & BUTTON_SELECT) {
        //buttonTone();
        tm.select();
        freeRAM();
      }
    }
    else if (settingBeep == 1) { //setting beep volume
      if (buttons & BUTTON_UP) {
        //buttonTone();
        increaseBeepVol();
        freeRAM();
      }
      if (buttons & BUTTON_DOWN) {
        //buttonTone();
        decreaseBeepVol();
        freeRAM();
      }
      if (buttons & BUTTON_LEFT) {
        ////buttonTone();
        freeRAM();
      }
      if (buttons & BUTTON_RIGHT) {
        ////buttonTone();
        freeRAM();
      }
      if (buttons & BUTTON_SELECT) {
        //buttonTone();
        tm.select();
        freeRAM();
      }
    }
    else if (settingSensitivity == 1) { //setting sensitivity
      if (buttons & BUTTON_UP) {
        //buttonTone();
        increaseSensitivity();
        freeRAM();
      }
      if (buttons & BUTTON_DOWN) {
        //buttonTone();
        decreaseSensitivity();
        freeRAM();
      }
      if (buttons & BUTTON_LEFT) {
        ////buttonTone();
        freeRAM();
      }
      if (buttons & BUTTON_RIGHT) {
        ////buttonTone();
        freeRAM();
      }
      if (buttons & BUTTON_SELECT) {
        //buttonTone();
        tm.select();
        freeRAM();
      }
    }
    else if (settingEcho == 1) { //setting echo protection
      if (buttons & BUTTON_UP) {
        //buttonTone();
        increaseEchoProtect();
        freeRAM();
      }
      if (buttons & BUTTON_DOWN) {
        //buttonTone();
        decreaseEchoProtect();
        freeRAM();
      }
      if (buttons & BUTTON_LEFT) {
        ////buttonTone();
        freeRAM();
      }
      if (buttons & BUTTON_RIGHT) {
        ////buttonTone();
        freeRAM();
      }
      if (buttons & BUTTON_SELECT) {
        //buttonTone();
        tm.select();
        freeRAM();
      }
    }
    else if (settingParState == 1) { //settingParState
      if (buttons & BUTTON_UP) {
        //buttonTone();
        toggleParState();
        freeRAM();
      }
      if (buttons & BUTTON_DOWN) {
        //buttonTone();
        toggleParState();
        freeRAM();
      }
      if (buttons & BUTTON_LEFT) {
        ////buttonTone();
        freeRAM();
      }
      if (buttons & BUTTON_RIGHT) {
        ////buttonTone();
        freeRAM();
      }
      if (buttons & BUTTON_SELECT) {
        //buttonTone();
        tm.select();
        freeRAM();
      }
    }
    else if (editingPar == 1) { //editing a Par time
      if (buttons & BUTTON_UP) {
        //buttonTone();
        increaseTime();
        freeRAM();
      }
      if (buttons & BUTTON_DOWN) {
        //buttonTone();
        decreaseTime();
        freeRAM();
      }
      if (buttons & BUTTON_LEFT) {
        //buttonTone();
        leftCursor();
        freeRAM();
      }
      if (buttons & BUTTON_RIGHT) {
        //buttonTone();
        rightCursor();
        freeRAM();
      }
      if (buttons & BUTTON_SELECT) {
        //buttonTone();
        editPar();
        freeRAM();
      }
    }
    else if (settingParTimes == 1) { //settingParState
      if (buttons & BUTTON_UP) {
        //buttonTone();
        parUp();
        freeRAM();
      }
      if (buttons & BUTTON_DOWN) {
        //buttonTone();
        parDown();
        freeRAM();
      }
      if (buttons & BUTTON_LEFT) {
        //buttonTone();
        tm.select();
        freeRAM();
      }
      if (buttons & BUTTON_RIGHT) {
        ////buttonTone();
        freeRAM();
      }
      if (buttons & BUTTON_SELECT) {
        //buttonTone();
        editPar();
        freeRAM();
      }
    }
    else  {                     //on the main menu
      if (buttons & BUTTON_UP) {
        //buttonTone();
        tm.prev();
        freeRAM();
      }
      if (buttons & BUTTON_DOWN) {
        //buttonTone();
        tm.next();
        freeRAM();
      }
      if (buttons & BUTTON_LEFT) {
        //buttonTone();
        tm.back();
        freeRAM();
      }
      if (buttons & BUTTON_RIGHT) {
        //buttonTone();
        tm.select(); //?? How will we make sure to render selected Menus off of the main area, while not allowing it to make MenuItems 'go'? Maybe checking whether the current item is a Menu or MenuItem? Is that possible? 
        freeRAM();
      }
      if (buttons & BUTTON_SELECT) {
        //buttonTone();
        tm.select();
        freeRAM();
      }
    }
  }
}








