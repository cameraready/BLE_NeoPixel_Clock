# BLE_NeoPixel_Clock

This is the Arduino code for the Bluetooth Low Energy clock project. It is designed to provide an interface between the RTC and the Arduino so that the clock can have the time updated, set the two alarms on the RTC and change the colors of the LEDs. There is a corresponding Android app to communicate through the BLE module.

Components used: 
* <a href="https://www.adafruit.com/products/2000">Adafruit Trinket Pro 5v</a>
* <a href="https://www.adafruit.com/products/1697">Adafruit NRF8001 BLE breakout board</a>
* <a href="https://www.adafruit.com/products/1460">Adafruit 30 LEDs per meter NeoPixels strip (2m)</a>
* <a href="http://macetech.com/store/index.php?main_page=product_info&products_id=8">Chronodot 2.1 RTC</a>
* 10v 4700uf capacitor
* Blue LED


Current version is still early Beta stage. Most of the functions are in place but need refined. The Arduino Uno has been replaced with an Adafruit Trinket Pro 5v with everything soldered to a perma-proto board.

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
* Get current temperature from RTC: `!T`

##TODO
* [x] Create variable to set `midnight` position relative to orientation of clock.
* [X] Save values to EEPROM for DST, alarm settings and other variables.
* [ ] Create method for sending current EEPROM values to Android app.
* [ ] Update Android app to query clock for current settings from EEPROM when establishing connection with Bluetooth.
* [ ] Update alarm and temp animation to non-blocking.
* [x] Add method to display current temperature on clock.