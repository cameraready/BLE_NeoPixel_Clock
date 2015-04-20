# BLE_NeoPixel_Clock

This is the Arduino code for the Bluetooth Low Energy clock project. It is designed to provide an interface between the RTC and the Arduino so that the clock can have the time updated, set the two alarms on the RTC and change the colors of the LEDs. There is a corresponding Android app to communicate through the BLE module.

Initial testing done with 
* Arduino Uno R3
* Adafruit NRF8001 BLE breakout board
* Adafruit 30 LEDs per meter NeoPixels strip (2m)
* Chronodot 2.1 RTC
* 10v 4700uf capacitor
* Blue LED


Current version is still early Beta stage. Most of the functions are in place but need refined. The Uno has been replaced with an Adafruit Trinket Pro 5v with everything soldered to a perma-proto board.

## UART codes for sending data
* color values `#<hour|min|sec><Red><Green><Blue>` with colors being 2 digit hex values
  - Example: set hour color to blue: `#H0000FF`
* update RTC `%yyyyMMdd HHmmss EEE`
  - Example: `%20150409 101500 Thu`

* Set Alarms `$<alarm num><enable><repeating>:<sec>:<min>:<hour>:<day>`
  - Example: set alarm 2 for every minute `$2tt8e:00:00:12:01`

* Get current RTC time in UTC: `&`

* Enable DST: `@<true|false>`
  - example enable DST: `@T`

* Location data from Android: `!L`
* Set `midnight` variable for clock orientation on LED strip `!M<00-59>`

##TODO
* [x] Create variable to set `midnight` position relative to orientation of clock.
* [ ] Update alarm animation to non-blocking
* [ ] Save settings to EEPROM for DST, Repeat and other variables