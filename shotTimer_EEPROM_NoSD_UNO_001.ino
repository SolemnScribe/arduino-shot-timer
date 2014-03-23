/////////////////////////////////
// Shot Timer rev. 3a
// copyright  2013, 2014 - Tim JH L
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

//PROGMEM
#include <avr/pgmspace.h>
//EEPROM
#include <EEPROM.h>

#include "pitches.h" // musical pitches - optional - format: NOTE_C4
#include <toneAC.h> //Bit-Bang tone library for piezo buzzer http://code.google.com/p/arduino-tone-ac/

//RGB LCD Shield Libraries
#include <Wire.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>
#include <MenuBackend.h> //Documentation: http://wiring.org.co/reference/libraries/MenuBackend/index.html
/*
IMPORTANT: to use the menubackend library by Alexander Brevig download it at http://www.arduino.cc/playground/uploads/Profiles/MenuBackend_1-4.zip (OLD)
 (Download the Latest version from WIRING - https://github.com/WiringProject/Wiring/tree/master/framework/libraries/MenuBackend 
 */

#include <StopWatch.h> //http://playground.arduino.cc/Code/StopWatchClass

//////////////
// Other code samples used:
// Adafruit sound level sampling: http://learn.adafruit.com/adafruit-microphone-amplifier-breakout/measuring-sound-levels
//////////////

//////////////
// DEFINITIONS
//////////////

// These #defines make it easy to set the backlight color
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

//////////////
// PROGMEM
//////////////

//PROGMEM String Conversion - http://electronics4dogs.blogspot.com/2010_12_01_archive.html
//#define P(str) (strcpy_P(p_buffer, PSTR(str)), p_buffer)
///char p_buffer[70]; //progmem P(STRINGS) CANNOT BE LONGER THAN THIS CHARACTER COUNT
// REDUNDANT WITH F() MACRO?

//Menu Names - format: const char Name[] PROGMEM = ""; <---- this doesn't work with menuBackEnd and I don't know why.
char startName[] = "[Start]";
char reviewName[] = "[Review]";
char parName[] = "Set Par >>";
char parSetName[] = "<< [Toggle Par]";
char parTimesName[] = "<< [Par Times]";
char settingsName[] = "Settings >>";
char setDelayName[] = "<< [Set Delay]";
char buzzerName[] = "<< [Buzzr Vol]";
char sensitivityName[] = "<< [Sensitivity]";
char echoName[] = "<< [Echo Reject]"; 

//////////////
// CONSTANTS
//////////////

// StopWatch
StopWatch shotTimer;

// The shield uses the I2C SCL and SDA pins. On classic Arduinos
// this is Analog 4 and 5 so you can't use those for analogRead() anymore
// However, you can connect other I2C sensors to the I2C bus and share
// the I2C bus.
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

const byte micPin = A0; // the mic/amp is connected to analog pin 0
//set the input for the mic/amplifier 

// set attributes for the button tones
const byte buttonVol = 5;
const byte buttonDur = 80;
const int buttonNote = NOTE_A4;

const int beepDur = 400;
const int beepNote = NOTE_C4;

const int shotLimit = 200;
const int parLimit = 10;

//////////////
// VARIABLES
//////////////
unsigned long shotTimes[shotLimit];
unsigned long parTimes[parLimit];
unsigned long additivePar = 0;

byte currentShot = 0;
byte reviewShot = 0;
byte currentPar = 0;
//int sensorReading = 0;      // variable to store the value read from the sensor pin
//int sample; // reading from the mic input (alternate) for function sampleSound() 


////////////////////////////////////////
// EEPROM: Settings to be stored in EEPROM?
////////////////////////////////////////
boolean useEEPROM = 1;  // ENABLE THIS TO ENABLE READ WRITE FROM EEPROM, EEPROM HAS 100,000 READ/WRITE CYCLES, conservatively
// http://tronixstuff.wordpress.com/2011/05/11/discovering-arduinos-internal-eeprom-lifespan/
byte delayTime = 11;    // EEPROM: 301
byte beepVol = 10;      // EEPROM: 302
byte sensitivity = 1;   // EEPROM: 303 
byte sampleWindow = 50; // EEPROM: 304  //  ECHO REJECT: Sample window width in mS (50 mS = 20Hz) for function sampleSound()
/////////////////////////////////////////


int threshold = 625;

byte parCursor = 1;

boolean reviewingShots = false;
boolean parEnabled = false;

boolean settingParState = false;
boolean settingParTimes = false;
boolean editingPar = false;
boolean settingDelay = false;
boolean settingBeep = false;
boolean settingSensitivity = false;
boolean settingEcho = false;


uint8_t oldButtons; //ButtonState

//Menu variables
MenuBackend menu = MenuBackend(menuUseEvent,menuChangeEvent);
//initialize menuitems
MenuItem menuStart   = MenuItem(menu, startName, 1);
MenuItem menuReview    = MenuItem(menu, reviewName, 1);
MenuItem menuPar    = MenuItem(menu, parName, 1);
MenuItem menuParState = MenuItem(menu, parSetName, 2);
MenuItem menuParTimes = MenuItem(menu, parTimesName, 2);
MenuItem menuSettings = MenuItem(menu, settingsName, 1);
MenuItem menuStartDelay = MenuItem(menu, setDelayName, 2);
MenuItem menuBuzzer = MenuItem(menu, buzzerName, 2);
MenuItem menuSensitivity = MenuItem(menu, sensitivityName, 2);
MenuItem menuEcho = MenuItem(menu, echoName, 2);

//MenuItems for 

//////////////
// FUNCTIONS                            // Convert these to a library? 
//////////////


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
  return(peakToPeak);
}

//////////////////////////////////////////////////////////
// Listen for Shots
//////////////////////////////////////////////////////////

void listenForShots(){
  if(sampleSound() >= threshold){
    recordShot();
  } 
}

//////////////////////////////////////////////////////////
// Start the Shot Timer
//////////////////////////////////////////////////////////

void startTimer() {
  lcd.setBacklight(GREEN); 
  shotTimer.reset(); //reset the timer to 0
  for (int c = 0; c < currentShot; c++){  // reset the values of the array of shots to 0 NOT <= because currentShot is incremented at the end of the last one recorded
    shotTimes[c] = 0; 
  }
  currentShot = 0; //start with the first shot in the array
  lcd.setCursor(0,0); 
  lcd.print(F("Wait for it...  "));
  lcd.setCursor(0,1); 
  lcd.print(F("                ")); // create a clearline function? Save fewer strings in progmem?
  startDelay();
  lcd.setCursor(0,0); 
  lcd.print(F("      GO!!      "));
  lcd.setCursor(0,1); 
  lcd.print(F("Last:")); //10 chars
  BEEP();
  shotTimer.start();
  serialPrint(shotTimer.elapsed(), 7);
}
//////////////////////////////////////////////////////////
// Stop the shot timer
//////////////////////////////////////////////////////////
void stopTimer(boolean out = 0) {
  if (out == 1){
    lcd.setBacklight(RED);
  }
  else {
    lcd.setBacklight(WHITE); 
  }
  shotTimer.stop();
  Serial.println(F("Timer was stopped at:")); 
  serialPrint(shotTimer.elapsed(), 7);
  for (int i = 0; i < 5; i++){
    toneAC(NOTE_C5, 9, 100, false);
    delay(50);  
  }
  if (out == 1){
    lcd.setBacklight(WHITE);
  }
  menu.moveDown(); //move the menu down to review mode
  reviewShots(); //move into shot review mode immediately 
} 

//////////////////////////////////////////////////////////
// record shots to the string array
//////////////////////////////////////////////////////////

void recordShot(){
  shotTimes[currentShot] = shotTimer.elapsed();
  //Serial.print(F("Shot #")); Serial.print(currentShot + 1); Serial.print(F(" - ")); serialPrint(shotTimes[currentShot], 7); 
  //Serial.println(shotTimer.elapsed());
  lcd.setCursor(6,1);
  lcdPrint(shotTimes[currentShot],7); //lcd.print(F(" ")); if(currentShot > 1) {lcdPrint(shotTimes[currentShot]-shotTimes[currentåShot-1],5);}
  //9 characters             //1 characters                    //6 characters
  currentShot += 1;
  if (currentShot == shotLimit){  // if the current shot == 100 (1 more than the length of the array)
    stopTimer(1);
  }
}

//////////////////////////////////////////////////////////
//review shots - initialize the shot review screen
//////////////////////////////////////////////////////////

void reviewShots(){
  reviewingShots = !reviewingShots;
  if (reviewingShots == 1){
    if (currentShot > 0) {
      reviewShot = currentShot - 1; 
    } 
    Serial.print(reviewShot);
    for (int t = 0; t < currentShot; t++){ 
      Serial.print(F("Shot #")); 
      Serial.print(t + 1); 
      Serial.print(F(" - "));
      serialPrint(shotTimes[t],7); 
    }
    lcd.setBacklight(VIOLET);
    lcd.setCursor(0,0);
    lcd.print(F("Shot #")); 
    lcd.print(currentShot); 
    lcd.print(F("                ")); 
    lcd.setCursor(9,0); 
    lcd.print(F(" Split "));
    lcd.setCursor(0,1);
    lcdPrint(shotTimes[reviewShot],7); 
    lcd.print(F(" ")); 
    if(reviewShot > 1) {
      lcdPrint(shotTimes[reviewShot]-shotTimes[reviewShot-1],5);
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

void nextShot(){
  if(currentShot == 0 || reviewShot == currentShot - 1){
    reviewShot = 0;
  } 
  else {
    reviewShot++;
  }
  lcd.setCursor(0,0);
  lcd.print(F("Shot #")); 
  lcd.print(reviewShot + 1); 
  lcd.print(F("                ")); 
  lcd.setCursor(9,0); 
  lcd.print(F(" Split "));
  lcd.setCursor(0,1); 
  lcdPrint(shotTimes[reviewShot],7); 
  lcd.print(F(" ")); 
  if(reviewShot == 0) {
    lcd.print(F("   1st"));
  } 
  else {
    lcdPrint(shotTimes[reviewShot]-shotTimes[reviewShot-1],5);
  }
}

//////////////////////////////////////////////////////////
// review shots - move to the previous shot in the string
//////////////////////////////////////////////////////////

void previousShot(){
  if (currentShot == 0){
    reviewShot = 0;
  }
  else if(reviewShot == 0){
    reviewShot = currentShot - 1;
  } 
  else {
    reviewShot--;
  }

  lcd.setCursor(0,0);
  lcd.print(F("Shot #")); 
  lcd.print(reviewShot + 1); 
  lcd.print(F("                ")); 
  lcd.setCursor(9,0); 
  lcd.print(F(" Split "));
  lcd.setCursor(0,1); 
  lcdPrint(shotTimes[reviewShot],7); 
  lcd.print(F(" ")); 
  if(reviewShot == 0) {
    lcd.print(F("   1st"));
  } 
  else {
    lcdPrint(shotTimes[reviewShot]-shotTimes[reviewShot-1],5);
  }
}

//////////////////////////////////////////////////////////
// Rate of Fire
//////////////////////////////////////////////////////////

void rateOfFire(boolean includeDraw = true){
  unsigned int rof;
  if (!includeDraw){
    rof = (shotTimes[currentShot-1] - shotTimes[0]) / (currentShot - 1);
  } 
  else rof = shotTimes[currentShot-1] / (currentShot - 1);

  lcd.setCursor(0,0);
  lcd.print(F("                "));
  lcd.setCursor(0,0);
  lcd.print(F("Avg Split:")); 
  lcd.setCursor(11,0); 
  lcdPrint(rof,5);
  lcd.setCursor(0,1);
  lcd.print(F("                "));
  lcd.setCursor(0,1);
  lcd.print(F("Shots/min:")); 
  lcd.setCursor(11,1); 
  lcd.print(60000/rof); // will this produce a decimal? Or a truncated int? 
  //10 characters   //1 characters          //4 characters plus truncated? 
}


/////////////////////////////////////////////////////////////
// setDelay
/////////////////////////////////////////////////////////////

void setDelay(){
  settingDelay = !settingDelay;
  if (settingDelay == 1){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(F("Start Delay")); 
    lcd.setCursor(0,1);
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
    if (useEEPROM == 1 && EEPROM.read(301) != delayTime) {
      Serial.println(F("Read from EEPROM 301"));
      EEPROM.write(301, delayTime); 
      Serial.println(F("Wrote to EEPROM 301"));
    }   
    returnToMenu(); 
  }
  Serial.println(settingDelay);
}


/////////////////////////////////////////////////////////////
// increaseDelay
/////////////////////////////////////////////////////////////

void increaseDelay() {
  if(delayTime == 12){
    delayTime = 0;
  } 
  else {
    delayTime++;
  }
  lcd.setCursor(0,1);
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
  if(delayTime == 0) {
    delayTime = 12;
  } 
  else {
    delayTime--;
  }
  lcd.setCursor(0,1);
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

void startDelay(){
  if (delayTime > 11) {
    delay(random(2000,6001)); //from 2 to 6 seconds
  } 
  else if (delayTime == 11) {
    delay(random(1000,4001)); //from 1 to 4 seconds
  } 
  else {
    delay(delayTime * 1000); //fixed number of seconds
  }   
}


/////////////////////////////////////////////////////////////
// setBeepVol
/////////////////////////////////////////////////////////////

void setBeepVol(){
  settingBeep = !settingBeep;
  if (settingBeep == 1){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(F("Buzzer Volume")); 
    lcd.setCursor(0,1);
    lcd2digits(beepVol);   
  } 
  else {
    if (useEEPROM == 1 && EEPROM.read(302) != beepVol) {
      Serial.println(F("Read from EEPROM 302"));
      EEPROM.write(302, beepVol); 
      Serial.println(F("Wrote to EEPROM 302"));
    }   
    returnToMenu(); 
  }
  Serial.println(settingBeep);
}


/////////////////////////////////////////////////////////////
// increaseBeepVol
/////////////////////////////////////////////////////////////

void increaseBeepVol() {
  if(beepVol == 10){
    beepVol = 0;
  } 
  else {
    beepVol++;
  }
  lcd.setCursor(0,1);
  lcd2digits(beepVol);
  lcd.print(F("                "));   
}

/////////////////////////////////////////////////////////////
// decreaseBeepVol
/////////////////////////////////////////////////////////////

void decreaseBeepVol() {
  if(beepVol == 0){
    beepVol = 10;
  } 
  else {
    beepVol--;
  }
  lcd.setCursor(0,1);
  lcd2digits(beepVol);
  lcd.print(F("                "));   
}


/////////////////////////////////////////////////////////////
// setSensitivity
/////////////////////////////////////////////////////////////

void setSensitivity(){
  settingSensitivity = !settingSensitivity;
  if (settingSensitivity == 1){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(F("Sensitivity")); 
    lcd.setCursor(0,1);
    lcd2digits(sensitivity);   
  } 
  else {
    if (useEEPROM == 1 && EEPROM.read(303) != sensitivity) {
      Serial.println(F("Read from EEPROM 303"));
      EEPROM.write(303, sensitivity); 
      Serial.println(F("Wrote to EEPROM 303"));
    }   
    returnToMenu(); 
  }
  Serial.println(settingSensitivity);
}


/////////////////////////////////////////////////////////////
// increaseSensitivity
/////////////////////////////////////////////////////////////

void increaseSensitivity() {
  if(sensitivity == 20){
    sensitivity = 0;
  } 
  else {
    sensitivity++;
  }
  sensToThreshold();
  lcd.setCursor(0,1);
  lcd2digits(sensitivity);
  lcd.print(F("                "));
}

/////////////////////////////////////////////////////////////
// decreaseSensitivity
/////////////////////////////////////////////////////////////

void decreaseSensitivity() {
  if(sensitivity == 0){
    sensitivity = 20;
  } 
  else {
    sensitivity--;
  }
  sensToThreshold();
  lcd.setCursor(0,1);
  lcd2digits(sensitivity);
  lcd.print(F("                "));
}


/////////////////////////////////////////////////////////////
// setEchoProtect - EEPROM
/////////////////////////////////////////////////////////////

void setEchoProtect(){
  settingEcho = !settingEcho;
  if (settingEcho == 1){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(F("Echo Protect")); 
    lcd.setCursor(0,1);
    lcd.print(sampleWindow); 
    lcd.print(F("ms"));  
  } 
  else { 
    if (useEEPROM == 1 && EEPROM.read(304) != sampleWindow) {
      Serial.println(F("Read from EEPROM 304"));
      EEPROM.write(304, sampleWindow);
      Serial.println(F("Wrote to EEPROM 304")); 
    }
    returnToMenu(); 
  }
  Serial.println(settingEcho);
}


/////////////////////////////////////////////////////////////
// increaseEchoProtect
/////////////////////////////////////////////////////////////

void increaseEchoProtect() {
  if(sampleWindow == 100){
    sampleWindow = 10;
  } 
  else {
    sampleWindow += 10;
  }
  lcd.setCursor(0,1);
  lcd.print(sampleWindow); 
  lcd.print(F("ms"));
  lcd.print(F("                ")); 
}

/////////////////////////////////////////////////////////////
// decreaseEchoProtect
/////////////////////////////////////////////////////////////

void decreaseEchoProtect() {
  if(sampleWindow == 0){
    sampleWindow = 100;
  } 
  else {
    sampleWindow -= 10;
  }
  lcd.setCursor(0,1);
  lcd.print(sampleWindow); 
  lcd.print(F("ms"));
  lcd.print(F("                ")); 
}

/////////////////////////////////////////////////////////////
// convert sensitivity to threshold
/////////////////////////////////////////////////////////////

void sensToThreshold(){
  threshold = 650-(25*sensitivity);
}


/////////////////////////////////////////////////////////////
// setParState
/////////////////////////////////////////////////////////////

void setParState(){
  settingParState = !settingParState;
  if (settingParState == 1){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(F("Par Times")); 
    lcd.setCursor(0,1);
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
  lcd.setCursor(0,1);
  if (parEnabled == false) {
    lcd.print(F("[DISABLED]")); //10 characters
  }
  else {
    lcd.print(F("[ENABLED] ")); //10 characters
  }
}

/////////////////////////////////////////////////////////////
// setParTimes
/////////////////////////////////////////////////////////////

void setParTimes(){
  settingParTimes = !settingParTimes;
  if (settingParTimes == 1){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(F("<<"));
    lcd.setCursor(5,0); 
    lcd.print(F("Par"));
    lcd.setCursor(9,0); 
    lcd2digits(currentPar + 1);
    lcd.setCursor(4,1); 
    if (currentPar > 0){
      lcd.print(F("+")); 
    } 
    else {
      lcd.print(F(" "));
    }
    lcdPrint(parTimes[currentPar],7);
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
  if(currentPar == 0){
    currentPar = parLimit - 1;
  } 
  else {
    currentPar--;
  }
  lcd.setCursor(9,0); 
  lcd2digits(currentPar + 1);
  lcd.setCursor(4,1); 
  if (currentPar > 0){
    lcd.print(F("+")); 
  }
  else {
    lcd.print(F(" "));
  }
  lcdPrint(parTimes[currentPar],7);
}

/////////////////////////////////////////////////////////////
// parDown()
/////////////////////////////////////////////////////////////

void parDown() {
  if(currentPar == parLimit - 1){
    currentPar = 0;
  } 
  else {
    currentPar++;
  }
  lcd.setCursor(9,0); 
  lcd2digits(currentPar + 1);
  lcd.setCursor(4,1); 
  if (currentPar > 0){
    lcd.print(F("+")); 
  } 
  else {
    lcd.print(F(" "));
  }
  lcdPrint(parTimes[currentPar],7);
}


/////////////////////////////////////////////////////////////
// editPar()
/////////////////////////////////////////////////////////////

void editPar(){
  settingParTimes = 0;
  editingPar = !editingPar;
  if (editingPar == 1){
    lcd.setBacklight(GREEN);
    lcd.setCursor(0,0); 
    lcd.print(F("Edit        "));
    lcd.setCursor(0,1); 
    lcd.print(F("P")); 
    lcd2digits(currentPar + 1); 
    if (currentPar > 0){
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
    setParTimes(); 
  }
  Serial.println(editingPar);
}

/////////////////////////////////////////////////////////////
// leftCursor()
/////////////////////////////////////////////////////////////

void leftCursor(){
  if (parCursor == 7){
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

void rightCursor(){
  if (parCursor == 1){
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
void lcdCursor(){
  switch(parCursor){
  case 1: //milliseconds
    lcd.setCursor(11,0);//icon at 13
    lcd.print(F("  _"));
    lcd.setCursor(5,0);//left behind icon at 5
    lcd.print(F(" "));
    break;
  case 2: //ten milliseconds
    lcd.setCursor(10,0);//icon at 12
    lcd.print(F("  _  "));
    break; 
  case 3: //hundred milliseconds
    lcd.setCursor(9,0);//icon at 11
    lcd.print(F("  _  "));
    break;
  case 4: //seconds 
    lcd.setCursor(7,0);//icon at 9
    lcd.print(F("  _  "));
    break;
  case 5: //ten seconds
    lcd.setCursor(6,0);// icon at 8
    lcd.print(F("  _  "));
    break;
  case 6: //minutes
    lcd.setCursor(4,0); //icon at 6
    lcd.print(F("  _  "));
    break;
  case 7: //ten minutes
    lcd.setCursor(5,0); //icon at 5
    lcd.print(F("_  "));
    lcd.setCursor(13,0);//left behind icon at 13
    lcd.print(F(" "));
    break;
  }
}

/////////////////////////////////////////////////////////////
// increaseTime()
/////////////////////////////////////////////////////////////

void increaseTime(){
  switch (parCursor){
  case 1: // milliseconds
    if (parTimes[currentPar] == 5999999){
      break; 
    } 
    else {
      parTimes[currentPar]++;
    }
    break; 
  case 2: // hundreds milliseconds
    if (parTimes[currentPar] >= 5999990){
      break; 
    } 
    else {
      parTimes[currentPar] += 10;
    }
    break; 
  case 3: // tens milliseconds
    if (parTimes[currentPar] >= 5999900){
      break; 
    } 
    else {
      parTimes[currentPar] += 100;
    }
    break;   
  case 4: // seconds
    if (parTimes[currentPar] >= 5999000){
      break; 
    } 
    else {
      parTimes[currentPar] += 1000;
    }
    break;
  case 5: // ten seconds
    if (parTimes[currentPar] >= 5990000){
      break; 
    } 
    else {
      parTimes[currentPar] += 10000;
    }
    break; 
  case 6: // minutes
    if (parTimes[currentPar] >= 5940000){
      break; 
    } 
    else {
      parTimes[currentPar] += 60000;
    }
    break;
  case 7: // 10 minutes
    if (parTimes[currentPar] >= 5400000){
      break; 
    } 
    else {
      parTimes[currentPar] += 600000;
    }
    break;   
  }
  lcd.setCursor(5,1);
  lcdPrint(parTimes[currentPar],7);
}

/////////////////////////////////////////////////////////////
// decreaseTime()
/////////////////////////////////////////////////////////////

void decreaseTime(){
  switch (parCursor){
  case 1:
    if (parTimes[currentPar] < 1){
      break; 
    } 
    else {
      parTimes[currentPar]--;
    }
    break; 
  case 2:
    if (parTimes[currentPar] < 10){
      break;
    } 
    else {
      parTimes[currentPar] -= 10;
    }
    break; 
  case 3:
    if (parTimes[currentPar] < 100){
      break;
    } 
    else {
      parTimes[currentPar] -= 100;
    }
    break;   
  case 4:
    if (parTimes[currentPar] < 1000){
      break;
    } 
    else {
      parTimes[currentPar] -= 1000;
    }
    break;
  case 5:
    if (parTimes[currentPar] < 10000){
      break;
    } 
    else {
      parTimes[currentPar] -= 10000;
    }
    break; 
  case 6:
    if (parTimes[currentPar] < 60000){
      break;
    } 
    else {
      parTimes[currentPar] -= 60000;
    }
    break;
  case 7:
    if (parTimes[currentPar] < 600000){
      break;
    } 
    else {
      parTimes[currentPar] -= 600000;
    }
    break;   
  }
  lcd.setCursor(5,1);
  lcdPrint(parTimes[currentPar],7);
}

//////////////////////////////////////////////////////////
// Close Enough Par Beep (adjustment for echoProtect sample window)
//////////////////////////////////////////////////////////


//boolean parBeep(byte i){
//  if (shotTimer.elapsed() <= (parTimes[i] + (sampleWindow / 2)) && shotTimer.elapsed() >= (parTimes[i] - sampleWindow / 2)){
//    return true;
//  } else {
//    return false;
//  }
//}

//////////////////////////////////////////////////////////
// Return to the menu screen
//////////////////////////////////////////////////////////

void returnToMenu(){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(F("Shot Timer v.2"));
  lcd.setCursor(0,1);
  lcd.print(menu.getCurrent().getName());
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
    x = t/ 3600000UL;
    t -= x * 3600000UL;
    serial2digits(x);
    Serial.print(':');
  }

  // MINUTES
  x = t/ 60000UL;
  t -= x * 60000UL;
  serial2digits(x);
  Serial.print(':');

  // SECONDS
  x = t/1000UL;
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
// Helper - 2 digits
/////////////////////////////////////////////////////////////

void serial2digits(uint32_t x)
{
  if (x < 10) Serial.print(F("0"));
  Serial.print(x);
}

/////////////////////////////////////////////////////////////
// helper - 3 digits
/////////////////////////////////////////////////////////////

void serial3digits(uint32_t x)
{
  if (x < 100) Serial.print(F("0"));
  if (x < 10) Serial.print(F("0"));
  Serial.print(x);
}

/////////////////////////////////////////////////////////////
// Helper - 4 digits
/////////////////////////////////////////////////////////////

void serial4digits(uint32_t x) {
  if (x < 1000) Serial.print(F("0"));
  if (x < 100) Serial.print(F("0"));
  if (x < 10) Serial.print(F("0"));
  Serial.print(x);
}


/////////////////////////////////////////////////////////////
// Print time to an LCD screen 
/////////////////////////////////////////////////////////////

void lcdPrint(uint32_t t, byte digits)
{
  uint32_t x;
  if (digits >= 8)  
  {
    // HOURS
    x = t/ 3600000UL;
    t -= x * 3600000UL;
    lcd2digits(x);
    lcd.print(':');
  }
  if (digits >= 6) {
    // MINUTES
    x = t/ 60000UL;
    t -= x * 60000UL;
    lcd2digits(x);
    lcd.print(':');
  }
  if (digits >= 4){
    // SECONDS
    x = t/1000UL;
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
  else if (digits >= 2){
    // ROUNDED HUNDREDTHS
    lcd.print('.');
    x = (t+5)/10L;  // rounded hundredths?
    lcd2digits(t);
  }
}

/////////////////////////////////////////////////////////////
// Helper - 2 digits
/////////////////////////////////////////////////////////////

void lcd2digits(uint32_t x)
{
  if (x < 10) lcd.print(F("0"));
  lcd.print(x);
}

/////////////////////////////////////////////////////////////
// helper - 3 digits
/////////////////////////////////////////////////////////////
void lcd3digits(uint32_t x)
{
  if (x < 100) lcd.print(F("0"));
  if (x < 10) lcd.print(F("0"));
  lcd.print(x);
}

/////////////////////////////////////////////////////////////
// helper - 4 digits
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

void BEEP(){
  toneAC(beepNote, beepVol, beepDur, true);
}

/////////////////////////////////////////////////////////////
// buttonTone
/////////////////////////////////////////////////////////////

void buttonTone() {
  toneAC(buttonNote, buttonVol, buttonDur, true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// SETUP FUNCTIONS
/////////////////////////////////////////////////////////////////////////////////////////////////////////

//this function builds the menu and connects the correct items together
void menuSetup()
{
  Serial.println(F("Setting up menu..."));
  //add the file menu to the menu root
  menu.getRoot().add(menuStart); 
  //setup the start menu item
  menuStart.addAfter(menuReview);  //loop up and down between start and review
  menuReview.addAfter(menuPar); 
  menuPar.addRight(menuParState);
  menuParState.addAfter(menuParTimes);
  menuPar.addAfter(menuSettings);
  menuSettings.addRight(menuStartDelay);
  menuStartDelay.addAfter(menuBuzzer);
  menuBuzzer.addAfter(menuSensitivity);
  menuSensitivity.addAfter(menuEcho);

  //addLefts - must appear in reverse order!
  menuParTimes.addLeft(menuPar);
  menuParState.addLeft(menuPar);

  menuEcho.addLeft(menuSettings);
  menuSensitivity.addLeft(menuSettings);
  menuBuzzer.addLeft(menuSettings);
  menuStartDelay.addLeft(menuSettings);

  //loopbacks
  menuSettings.addAfter(menuStart);
  menuParTimes.addAfter(menuParState);
  menuEcho.addAfter(menuStartDelay);
  /* INSERT FOR LOOP OF MENU ITEMS FOR STRINGS PRESENT ON SD CARD ?? OR ADD THESE ITEMS IN LOOP? HOW DO WE RETRIEVE REVIEW STRINGS AFTER JUST RECORDED?*/

  menu.moveDown();
}


void lcdSetup() {
  lcd.begin(16, 2);
  // Print a message to the LCD. We track how long it takes since
  // this library has been optimized a bit and we're proud of it :)
  int time = millis();
  lcd.print(F("Shot Timer v.2"));
  lcd.setCursor(0,1);
  lcd.print(F("[Start]         "));
  time = millis() - time;
  Serial.print(F("Took ")); 
  Serial.print(time); 
  Serial.println(F(" ms"));
  lcd.setBacklight(WHITE);  

}

/*
  This is an important function
 Here all use events are handled
 
 This is where you define a behaviour for a menu item
 */


/////////////////////////////////////////////////////////////
// eepromSetup
/////////////////////////////////////////////////////////////

void eepromSetup(){
  byte dt = EEPROM.read(301);    // EEPROM: 301
  Serial.println(F("Read from EEPROM 301"));
  byte bv = EEPROM.read(302);      // EEPROM: 302
  Serial.println(F("Read from EEPROM 302"));
  byte st = EEPROM.read(303);  // EEPROM: 303
  Serial.println(F("Read from  EEPROM 303"));
  byte sw = EEPROM.read(304);  // EEPROM: 304
  Serial.println(F("Read from EEPROM 304"));

  //use settings values from EEPROM only if the read values are valid
  if (dt <= 12){ 
    delayTime = dt;
  }
  if (bv <= 10) {
    beepVol = bv;
  }
  if (st <= 20) {
    sensitivity = st;
  }
  if (sw <= 100) {
    sampleWindow = sw;
  }
  sensToThreshold(); //make sure that the Threshold is calculated based on the stored sensitivity setting
}

/////////////////////////////////////////////////////////////
// MENU STATE CHANGES
/////////////////////////////////////////////////////////////

void menuUseEvent(MenuUseEvent used)
{
  //DEBUG: Print used menu item to serial output
  Serial.print(F("Menu used: "));
  Serial.println(used.item.getName());

  //menuStart
  if (used.item.isEqual(menuStart)) //comparison agains a known item
  {
    Serial.println(F("Start Timer Function called here"));
    startTimer();
  }

  //menuReview
  if (used.item.isEqual(menuReview)) //comparison agains a known item
  {
    Serial.println(F("Review Function called here?"));
    reviewShots();
  }

  //menuPar
  if (used.item.isEqual(menuPar)) //comparison agains a known item
  {
    Serial.println(F("Par Function called here?"));
  }
  //menuParState
  if (used.item.isEqual(menuParState)) //comparison agains a known item
  {
    setParState();
    Serial.println(F("ParState Function called here?"));
  }

  //menuParTimes
  if (used.item.isEqual(menuParTimes)) //comparison agains a known item
  {
    setParTimes();
    Serial.println(F("ParTimes Function called here?"));
  }

  //menuSettings
  if (used.item.isEqual(menuSettings)) //comparison agains a known item
  {
    Serial.println(F("Settings Function called here?"));
  }

  //menuStartDelay
  if (used.item.isEqual(menuStartDelay)) //comparison agains a known item
  {
    Serial.println(F("Start Delay Function called here?"));
    setDelay();
  }

  //menuBuzzer
  if (used.item.isEqual(menuBuzzer)) //comparison agains a known item
  {
    Serial.println(F("Buzzer Function called here?"));
    setBeepVol();
  }

  //sensitivity
  if (used.item.isEqual(menuSensitivity)) //comparison agains a known item
  {
    Serial.println(F("Sensitivity Function called here?"));
    setSensitivity();
  }

  //echoProtect
  if (used.item.isEqual(menuEcho)) //comparison agains a known item
  {
    Serial.println(F("Sensitivity Function called here?"));
    setEchoProtect();
  }

}


/*
  This is an important function
 Here we get a notification whenever the user changes the menu
 That is, when the menu is navigated
 */
void menuChangeEvent(MenuChangeEvent changed)
{
  Serial.print(F("Menu change from "));
  Serial.print(changed.from.getName());
  Serial.print(F(" to "));
  Serial.println(changed.to.getName()); //changed.to.getName()
  lcd.setCursor(0,1);
  lcd.print(changed.to.getName()); 
  lcd.print(F("            ")); //12 spaces
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

void setup(){
  /////////////  
  // Debugging:
  /////////////

  //Open serial communications and wait for port to open:
  Serial.begin(9600);
  //    while (!Serial) {
  //      ; // wait for serial port to connect. Needed for Leonardo only
  //    }

  randomSeed(analogRead(1));

  if (useEEPROM == 1){
    Serial.print(F("Retrieveing prefs from EEPROM.."));
    eepromSetup();
  }

  lcdSetup(); 

  menuSetup();

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
  uint8_t buttons = newButtons & ~oldButtons;
  oldButtons = newButtons;


  if(shotTimer.isRunning()){ //listening for shots
    listenForShots();

    if (parEnabled == 1){
      additivePar = 0;

      for (byte i = 0; i < parLimit; i++){
        if (parTimes[i] == 0){ 
          break; 
        }
        additivePar += parTimes[i]; // add the parTimes together
        if (shotTimer.elapsed() <= (additivePar + (sampleWindow / 2)) && shotTimer.elapsed() >= (additivePar - sampleWindow / 2)){
          BEEP();  //Beep if the current time matches (within the boundaries of sample window) the parTime
        }
      }

    }
  }

  if (buttons){  
    if (shotTimer.isRunning()){ //while timer is running
      if (buttons & BUTTON_SELECT) {
        stopTimer();
      }
    }

    else  if (reviewingShots == 1){ //reviewing shots    /replace booleans for menus with a switch state based on a string or even based on a byte with values predefined? 
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
        menu.use();
        freeRAM();
      }
    }
    else if (settingDelay == 1){ //setting delay
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
        menu.use();
        freeRAM();
      }      
    }
    else if (settingBeep == 1){ //setting beep volume  
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
        menu.use();
        freeRAM();
      }      
    }
    else if (settingSensitivity == 1){ //setting sensitivity
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
        menu.use();
        freeRAM();
      }      
    }
    else if (settingEcho == 1){ //setting echo protection
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
        menu.use();
        freeRAM();
      }      
    }
    else if (settingParState == 1){ //settingParState
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
        menu.use();
        freeRAM();
      }      
    }
    else if (editingPar == 1){ //editing a Par time
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
    else if (settingParTimes == 1){ //settingParState
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
        menu.use();
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
        menu.moveUp();
        freeRAM();
      }
      if (buttons & BUTTON_DOWN) {
        //buttonTone();
        menu.moveDown();
        freeRAM();
      }
      if (buttons & BUTTON_LEFT) {
        //buttonTone();
        menu.moveLeft();
        freeRAM();
      }
      if (buttons & BUTTON_RIGHT) {
        //buttonTone();
        menu.moveRight();
        freeRAM();
      }
      if (buttons & BUTTON_SELECT) {
        //buttonTone();
        menu.use();
        freeRAM();
      }
    }
  }
}







