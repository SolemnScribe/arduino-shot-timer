////////////////////////////////////////////////////////////
// SD Helpers
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

#ifndef SDHELPERS_H_
#define SDHELPERS_H_

  //////////////////////////////
  // SD Check
  //////////////////////////////
  
  void SDCheck(Sd2Card* sd_card, SdVolume* sd_volume, SdFile* sd_root,
               int* kChipSelect, boolean* g_sd_present) {
    Serial.begin(9600);
    Serial.print(F("Initializing SD card..."));
    // On the Ethernet Shield, CS is pin 4. It's set as an output by default.
    // Note that even if it's not used as the CS pin, the hardware SS pin 
    // (10 on most Arduino boards, 53 on the Mega) must be left as an output 
    // or the SD library functions will not work. 
    pinMode(10, OUTPUT);     // change this to 53 on a mega
  
  
    // we'll use the initialization code from the utility libraries
    // since we're just testing if the card is working!
    if (!sd_card->init(SPI_HALF_SPEED, *kChipSelect)) {
      Serial.println(F("initialization failed. Things to check:"));
      Serial.println(F("* is a card is inserted?"));
      Serial.println(F("* Is your wiring correct?"));
      Serial.println(F("* did you change the chipSelect pin to match your shield or module?"));
      return;
    } else {
     Serial.println(F("Wiring is correct and a card is present.")); 
     *g_sd_present = true;
    }
  
    // print the type of card
    Serial.print(F("\nCard type: "));
    switch(sd_card->type()) {
      case SD_CARD_TYPE_SD1:
        Serial.println(F("SD1"));
        break;
      case SD_CARD_TYPE_SD2:
        Serial.println(F("SD2"));
        break;
      case SD_CARD_TYPE_SDHC:
        Serial.println(F("SDHC"));
        break;
      default:
        Serial.println(F("Unknown"));
    }
  
    // Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
    if (!sd_volume->init(sd_card)) {
      Serial.println(F("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card"));
      return;
    }
  
    // print the type and size of the first FAT-type volume
    uint32_t sd_volume_size;
    Serial.print(F("\nVolume type is FAT"));
    Serial.println(sd_volume->fatType(), DEC);
    Serial.println();
    
    sd_volume_size = sd_volume->blocksPerCluster();    // clusters are collections of blocks
    sd_volume_size *= sd_volume->clusterCount();       // we'll have a lot of clusters
    sd_volume_size *= 512;                            // SD card blocks are always 512 bytes
    Serial.print(F("Volume size (bytes): "));
    Serial.println(sd_volume_size);
    Serial.print(F("Volume size (Kbytes): "));
    sd_volume_size /= 1024;
    Serial.println(sd_volume_size);
    Serial.print(F("Volume size (Mbytes): "));
    sd_volume_size /= 1024;
    Serial.println(sd_volume_size);
  
    
    Serial.println(F("\nFiles found on the card (name, date and size in bytes): "));
    sd_root->openRoot(sd_volume);
    
    // list all files in the card with date and size
    sd_root->ls(LS_R | LS_DATE | LS_SIZE);
  }
  
  
  //////////////////////////////
  // ReadSDSettings
  // https://www.reddit.com/r/arduino/comments/2zzd41/need_help_combining_2_char_arrays/cpnqvfz
  //////////////////////////////
  void ReadSDSettings(Sd2Card* sd_card, SdFile* sd_root){
    char character;
    char setting_name[17] = "";
    char setting_value[3] = "";
    File sd_file = sd_root->open("ShotTimer/settings.st");
    if (sd_file) {
      while (sd_file->available()) {
        character = sd_file.read();
        while((sd_file.available()) && (character != '[')){
          character = sd_file.read();
        }
        character = sd_file.read();
        while((sd_file.available()) && (character != '=')){
          strcat(setting_name, character); 
          //settingName = settingName + character;
          character = sd_file.read();
        }
        character = sd_file.read();
        while((sd_file.available()) && (character != ']')){
          strcat(setting_value, character);
          //settingValue = settingValue + character;
          character = sd_file.read();
        }
        if(character == ']'){
         /*
         //Debuuging Printing
         Serial.print("Name:");
         Serial.println(settingName);
         Serial.print("Value :");
         Serial.println(settingValue);
         */
         // Apply the value to the parameter
          applySetting(setting_name,setting_value);
         // Reset Strings
          setting_name = "";
          setting_value = "";
          }
      }
      // close the file:
      sd_file.close();
    } else {
    // if the file didn't open, print an error:
    Serial.println("error opening settings.st");
    }
  }
  
  //////////////////////////////
  // ApplySettings
  // http://overskill.alexshu.com/saving-loading-settings-on-sd-card-with-arduino/ 
  // byte g_delay_time = 11;
  // byte g_beep_vol = 10;
  // byte g_sensitivity = 1;
  // byte g_sample_window = 50;
  //////////////////////////////
   void ApplySetting(char setting_name[], char setting_value[]) {
    
      if (strcmp(setting_name,"g_delay_time") == 0){
        g_delay_time = atoi(setting_value); 
      } else if (strcmp(setting_name,"g_beep_vol") == 0){
        g_beep_vol = atoi(setting_value);
      } else if (strcmp(setting_name,"g_sensitivity") == 0){
        g_sensitivity = atoi(setting_value);
      } else if (strcmp(setting_name,"g_sample_window") == 0){
        g_sample_window = atoi(setting_value);
      } else
        Serial.println(F("That's not a valid setting_name")); 

   }
  
  
  //////////////////////////////
  // WriteSDSettings
  // http://overskill.alexshu.com/saving-loading-settings-on-sd-card-with-arduino/ 
  //////////////////////////////
  
  void WriteSDSettings(Sd2Card* sd_card, SdFile* sd_root) {
    // Delete the old One
    sd_card->remove("settings.txt");
    // Create new one
    File sd_file = sd_root->open("settings.txt", FILE_WRITE);
    // writing in the file works just like regular print()/println() function
    sd_file.print("[");
    sd_file.print("exINT=");
    sd_file.print(exINT);
    sd_file.println("]");
    sd_file.print("[");
    sd_file.print("exFloat=");
    sd_file.print(exFloat,5);
    sd_file.println("]");
    sd_file.print("[");
    sd_file.print("exBoolean=");
    sd_file.print(exBoolean);
    sd_file.println("]");
    sd_file.print("[");
    sd_file.print("exLong=");
    sd_file.print(exLong);
    sd_file.println("]");
    // close the file:
    sd_file.close();
    //Serial.println("Writing done.");
  }

  //////////////////////////////
  // converting string to Float
  //////////////////////////////
  float toFloat(String settingValue){
    char floatbuf[settingValue.length()+1];
    settingValue.toCharArray(floatbuf, sizeof(floatbuf));
    float f = atof(floatbuf);
    return f;
  }

  //////////////////////////////
  // converting string to Long
  //////////////////////////////
  long toLong(String settingValue){
    char longbuf[settingValue.length()+1];
    settingValue.toCharArray(longbuf, sizeof(longbuf));
    long l = atol(longbuf);
    return l;
  }

  //////////////////////////////
  // Converting String to integer and then to boolean
  // 1 = true
  // 0 = false
  //////////////////////////////
  boolean toBoolean(String settingValue) {
    if(settingValue.toInt()==1){
      return true;
    } else {
      return false;
    }
  }


#endif // SDHELPERS_H_
