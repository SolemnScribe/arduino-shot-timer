# arduino-shot-timer
#### Shot Timer
**Author**: hestenet  
**Canonical Repository**: https://github.com/hestenet/arduino-shot-timer  
**License**: LGPL 3  

## Documentation

### Features
- A timer that listens for shots, recording up to 200 shots in a string, for strings lasting into double-digit hours. 
- A review mode which displays the shot #, time, split time with previous shot.
  - Review mode can also display average split and rate of fire in shots/min
- User configurable par times
  - Ability to toggle the par times on and off
  - Ability to set up to 10 additive par beep times
- Setting for start delay, including: Random 1-4s, Random 2-6s, and 0-10 seconds 
- Setting for rate of fire - calculation can be configured to include the draw (time to first shot) or not
- Setting for beep volume
- Setting for sensitivity (with options that will work for dry fire)
- Setting for echo rejection
- Settings are preserved when the device is turned off (`EEPROM`)

### Hardware
- Arduino Uno R3
- [Adafruit RGB LCD Shield](https://www.adafruit.com/products/714)
- [Adafruit Electet Mic/Amp](https://www.adafruit.com/products/1063)
- Piezzo Buzzer

### Known Issues
- Enabling `DEBUG` with the present code results in OOM errors(too many `DEBUG` statements), particularly when editing par times, but possibly also when listening for shots. 

## Release History 

### [2.0.0] (https://github.com/hestenet/arduino-shot-timer/releases/tag/v.2.0.0) - EEPROM
The 2.x branch of Shot Timer stores its settings in EEPROM, and does not require an SD card interface for Arduino.

This release represents a major refactor of the code, with a few goals:
- Compatibility with Arduino v.1.6.8 
- Replace libraries with ones easily available in the `Arduino->Sketch->Include Libraries` interface
  - `<MenuSystem.h>` replaces `<MenuBackend.h>`
  - `<Chrono.h>` (specifically `LightChrono`) replaces `<StopWatch.h>`
  - `<PGMWrap.h>` has been added to help store strings and thus save dynamic memory
  - `<EEWrap.h>` has been added to improve `EEPROM` saving of settings
  - `<Adafruit_RGBLCDShield.h>` is unchanged
  - `<Wire.h>` is unchanged
  - `<avr/pgmspace.h>` is unchanged
  - `<toneAC.h>` is unchanged - and is now the only library from an external source:  https://bitbucket.org/teckel12/arduino-toneac/
- Pull some of my own functions into internal libraries
  - `"DebugMacros.h"` has been added to easily allow toggling of debug functions by commenting/uncommenting a single line
  - `"Pitches.h"` is unchanged
  - `"LegibleTime.h"` has been added to handle all functions of converting a time in `millis();` to a legible string in format: `00:00:00.000` to a specified number of digits.
  * `"LCDHelpers.h"` has been added to handle printing data from PROGMEM to the LCD, printing legible time to the LCD, and printing numbers to a specified number of digits to the LCD 
- Reduce overhead
  - v.2.0.0: **17,652 bytes (54%)** of PROGMEM and **1,467 bytes (71%)** of SRAM   (without `DEBUG`)
    - **Note**: The majority of this overhead is in the reservation of `uint32_t shotTimes[200]` (800 bytes) and in the reservation of `uint32_t parTimes[10]` (40 bytes) . Without these the program consumes **627 bytes (31%)** of SRAM.

### Deprecated Releases
#### ~~[1.0.0](https://github.com/hestenet/arduino-shot-timer/releases/tag/v1.0.0)~~

Initial build, before the migration from code.google.com to github. 
There are some incompatibilities with the latest Arduino IDE version in the menuBackend library used in this branch.
