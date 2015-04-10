/*********************************************************************
This is an example for our nRF8001 Bluetooth Low Energy Breakout

  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/products/1697

Adafruit invests time and resources providing this open source code, 
please support Adafruit and open-source hardware by purchasing 
products from Adafruit!

Written by Kevin Townsend/KTOWN  for Adafruit Industries.
MIT license, check LICENSE for more information
All text above, and the splash screen below must be included in any redistribution

This version uses call-backs on the event and RX so there's no data handling in the main loop!

~ Ross Heironimus ~
The basis of this code is the Adafruit sample code for the Bluetooth Low
Energy breakout board. The remainder of the code comes from some of my
previous clock projects.
1/10/15 - Changed to DS3232RTC library to allow setting alarms
1/15/15 - Seem to have memory collision issue around 19,400 bytes compiled
1/15/15 - Removed 4 Neopixels and using regular blue LED for bluetooth state
1/18/15 - Removed TimeZone library to save memory
3/11/15 - Changed time format string to include day of week
*********************************************************************/

/********************************************************************
* TODO:
*  Create helper methods to set alarm 1 and alarm 2
*  Create method to update date time in RTC from Blutooth module
*  
*  Create helper method for setting time from bluetooth
*  Replace DS3232RTC library with RTC_DS3231 library and compare performance
*  DS3232RTC lib has huge sync issues and slowly looses time between arduino and RTC
*********************************************************************/

/********************************************************************
* UART codes for sending data
* color values #<hour|min|sec><Red><Green><Blue> with colors being 2 digit hex values
*   example set hour color to blue: #H0000FF
* update RTC %yyyyMMdd HHmmss EEE
* Set Alarms $<alarm num><enable><repeating>:<sec>:<min>:<hour>:<day>
*    example set alarm 2 for every minute $2tt8e:00:00:12:01
* Get current RTC time in UTC: &
* Enable DST: @<true|false>
*    example enable DST: @T
* Location data from Android: !L
*********************************************************************/

#include <Wire.h>
#include <SPI.h>
#include <DS3232RTC.h>    //http://github.com/JChristensen/DS3232RTC
//#include <Timezone.h>     //https://github.com/JChristensen/Timezone
#include <Time.h>
#include <Adafruit_BLE_UART.h>
#include <Adafruit_NeoPixel.h>

#define ADAFRUITBLE_REQ 10
#define ADAFRUITBLE_RDY 3        // Changed from 2 to 3 for Trinket Pro compatibility
#define ADAFRUITBLE_RST 9
#define LED 8                    // switch Bluetooth module back to a LED
#define PIN2 5                   // Clock strip with 60 NeoPixels

Adafruit_BLE_UART uart = Adafruit_BLE_UART(ADAFRUITBLE_REQ, ADAFRUITBLE_RDY, ADAFRUITBLE_RST);
aci_evt_opcode_t prevState = ACI_EVT_DISCONNECTED;
//RTC_DS3231 RTC;                  // Set up real time clock
int LEDperiod = 0;   // Time (milliseconds) between LED toggles
boolean LEDstate = LOW; // LED flashing state HIGH/LOW
unsigned long prevLEDtime = 0L;    // For LED timing
unsigned long prevRTCtime = 0L;    // For RTC timing
uint32_t color            = 0x22;  // Used to set bluetooth status pixel colors
uint32_t hour_color       = 0x80;      // Blue
uint32_t minute_color     = 0x8000;    // Green
uint32_t second_color     = 0x800000;  // Red

//US Eastern Time Zone (Indianapolis)
//TimeChangeRule myDST = {"EDT", Second, Sun, Mar, 2, -240};    //Daylight time = UTC - 4 hours
//TimeChangeRule mySTD = {"EST", First, Sun, Nov, 2, -300};     //Standard time = UTC - 5 hours
//Timezone myTZ(myDST, mySTD);
//TimeChangeRule *tcr;        //pointer to the time change rule, use to get TZ abbrev
time_t utc, local;
boolean dst;                 // boolean value for calculation DST offset (0 = STD, 1 = DST)
uint8_t localOffset;         // Standard time offset from UTC for local time
uint8_t prevSec;             // debug current time on serial console once per second
boolean enableDST = true;
boolean alarmActive1 = false;
boolean alarmActive2 = false;
boolean repeatAlarm1 = false;  // set alarm 1 to not repeat
boolean repeatAlarm2 = false;  // set alarm 2 to not repeat

// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel clock = Adafruit_NeoPixel(60, PIN2, NEO_GRB + NEO_KHZ800);

static const uint32_t PROGMEM colors[] = {
  0x00FF00, 0x0CF300, 0x18E700, 0x24DB00, 0x33CC00, 0x3FC000, 
  0x4BB400, 0x57A800, 0x669900, 0x728D00, 0x7E8100, 0x8A7500, 
  0x996600, 0xA55A00, 0xB14E00, 0xC03F00, 0xCC3300, 0xD82700, 
  0xE41B00, 0xF30C00, 0xFF0000, 0xF3000C, 0xE70018, 0xD80027, 
  0xCC0033, 0xC0003F, 0xB4004B, 0xA5005A, 0x990066, 0x8D0072, 
  0x7E0081, 0x72008D, 0x660099, 0x5A00A5, 0x4B00B4, 0x3F00C0, 
  0x3300CC, 0x2700D8, 0x1800E7, 0x0C00F3, 0x0000FF, 0x000CF3, 
  0x001BE4, 0x0027D8, 0x0033CC, 0x0042BD, 0x004EB1, 0x005AA5, 
  0x006699, 0x00758A, 0x00817E, 0x008D72, 0x009966, 0x00A857, 
  0x00B44B, 0x00C03F, 0x00CC33, 0x00DB24, 0x00E718, 0x00F30C, 
};

// Animation for change of hour
void colorWipe(uint8_t h) {
  clock.clear();
  for (uint8_t i = 0; i<clock.numPixels(); i++) {
    clock.setPixelColor(i, pgm_read_dword(&colors[i]));
    clock.show();
    delay(16);
  }
  delay(250);
  // clear display before sweeping hours
  for (uint8_t i = 0; i<clock.numPixels(); i++) {
      clock.setPixelColor(i, 0);
      delay(16);
      clock.show();
  }
  
  // sweep hours up to current hour
  //if (h == 0) {h=12;} // Special case for noon and midnight
  
  for(uint8_t i=0; i<clock.numPixels(); i++) {
    if (i<=h && i!=0) // clear the first pixel and start showing hours at #1
      clock.setPixelColor(i, hour_color);
    else
      clock.setPixelColor(i, 0);
    clock.show();
    delay(16); // 1000/60 = 16 ms
  }
  delay(1000);
  
  for (uint8_t i = 0; i<clock.numPixels(); i++) {
      clock.setPixelColor(i, 0);
      delay(16);
      clock.show();
  }
}

/*************************************************************************
    This function is called whenever select ACI events happen
**************************************************************************/
void aciCallback(aci_evt_opcode_t event)
{
  switch(event)
  {
    case ACI_EVT_DEVICE_STARTED:
      Serial.println(F("Advertising started"));
      LEDperiod = 1000L / 10;
      break;
    case ACI_EVT_CONNECTED:
      Serial.println(F("Connected!"));
      LEDperiod = 1000L / 2;
      break;
    case ACI_EVT_DISCONNECTED:
      Serial.println(F("Disconnected or advertising timed out"));
      LEDperiod = 0L;
      break;
    default:
      break;
  }
}

/*************************************************************************
    This function is called whenever data arrives on the RX channel
**************************************************************************/
void rxCallback(uint8_t *buffer, uint8_t len)
{
  Serial.print(F("Received "));
  Serial.print(len);
  Serial.print(F(" bytes: "));
  for(int i=0; i<len; i++)
   Serial.print((char)buffer[i]); 

  Serial.print(F(" ["));

  for(int i=0; i<len; i++)
  {
    Serial.print(" 0x"); Serial.print((char)buffer[i], HEX); 
  }
  Serial.println(F(" ]"));
  
  switch (buffer[0])
  {
    case '#': {
      color = clock.Color(
            (unhex(buffer[2]) << 4) + unhex(buffer[3]),
            (unhex(buffer[4]) << 4) + unhex(buffer[5]),
            (unhex(buffer[6]) << 4) + unhex(buffer[7]));
      if (toupper((char)buffer[1]) == 'H') {
        Serial.print(F("Hour color rcvd! 0x"));
        Serial.println(color, HEX);
        uart.print("Hour color rcvd!");
        uart.write(buffer, len);
        hour_color = color;
      } else if (toupper((char)buffer[1]) == 'M') {
        Serial.print(F("Min color rcvd! 0x"));
        Serial.println(color, HEX);
        uart.print("Min color rcvd!");
        uart.write(buffer, len);
        minute_color = color;
      } else {
        Serial.print(F("Sec color rcvd! 0x"));
        Serial.println(color, HEX);
        uart.print("Sec color rcvd!");
        uart.write(buffer, len);
        second_color = color;
      } }
       break; 
    case '%': {
      Serial.println(F("Time received!"));
      //uint16_t yr = (unhex(buffer[1])*1000) + (unhex(buffer[2])*100) + (unhex(buffer[3])*10) + unhex(buffer[4]);
      //uint8_t  mo = (unhex(buffer[6])*10) + unhex(buffer[7]);
      //uint8_t  dy = (unhex(buffer[9])*10) + unhex(buffer[10]);
      //uint8_t  hr = (unhex(buffer[12])*10) + unhex(buffer[13]);
      //uint8_t  m  = (unhex(buffer[15])*10) + unhex(buffer[16]);
      //uint8_t  s  = (unhex(buffer[18])*10) + unhex(buffer[19]);
      //DateTime dt = DateTime(yr, mo, dy, hr, m, s);
      // Set system time first allows for immediate changes in time
      // just setting RTC time creates a delay between system time and RTC
      //setTime(hr, m, s, dy, mo, yr);   //set the system time
      //RTC.set(now());                  //set RTC time from system time
      tmElements_t tm;
      tm.Year =   (unhex(buffer[1])*1000) + (unhex(buffer[2])*100) + (unhex(buffer[3])*10) + unhex(buffer[4])-1970;
      tm.Month =  (unhex(buffer[5])*10) + unhex(buffer[6]);
      tm.Day =    (unhex(buffer[7])*10) + unhex(buffer[8]);
      // Get day of week
      switch (buffer[17]) {
        case 'S': {if (buffer[18] == 'u') tm.Wday = 1;
          else tm.Wday = 7;}
          break;
        case 'M': tm.Wday = 2;
          break;
        case 'T': {if (buffer[18] == 'u') tm.Wday = 3;
          else tm.Wday = 5;}
          break;
        case 'W': tm.Wday = 4;
          break;
        case 'F': tm.Wday = 6;
          break;
        default: tm.Wday = 7;
          break;
      }
      tm.Hour =   (unhex(buffer[10])*10) + unhex(buffer[11]);
      tm.Minute = (unhex(buffer[12])*10) + unhex(buffer[13]);
      tm.Second = (unhex(buffer[14])*10) + unhex(buffer[15]);
      RTC.write(tm);
      printRTCTime(tm);                  //debug time

      uart.print("Time received!");
      uart.write((uint8_t *)"\r\n", 2);
      uart.write(buffer, len); }
      break;
    case '&': {
      tmElements_t tm;
      RTC.read(tm);
      printRTCTime(tm);
      uart.print("Current DateTime:");
      uart.write((uint8_t *)"\r\n", 2);
      uart.print(getRTCTime(tm));
    }
      break;
    // Set Alarm $<num><enable><repeating>...
    case '$': {
      parseAlarm(buffer); }
      break;
    case '@': {
      if (toupper((char)buffer[1]) == 'T') {
        enableDST = true;
      } else {
        enableDST = false;
      }
      Serial.print(F("Enable DST: "));
      Serial.println(enableDST);
      uart.print("En/dis DST");
    }
      break;
    case '!': if (buffer[1] == 'L'){
      //if (checkCRC(buffer) == false) { break; }
      Serial.println(F("Location Data:"));
      printLocationData(buffer);
    }
      break;
    default: {
    /* Echo the same data back! */
      uart.write(buffer, len);
    }
      break;
  }
}

// Method to print GPS data from Android app
void printLocationData(uint8_t *buffer) {

  float x = *( (float*)(buffer + 2) );
  Serial.print(F("lat = "));
  Serial.println(x, 7);

  float y = *( (float*)(buffer + 6) );
  Serial.print(F("lng = "));
  Serial.println(y, 7);

  float z = *( (float*)(buffer + 10) );
  Serial.print(F("alt = "));
  Serial.println(z, 7); 

}

// Given hexadecimal character [0-9,a-f], return decimal value (0 if invalid)
uint8_t unhex(char c) {
  return ((c >= '0') && (c <= '9')) ?      c - '0' :
         ((c >= 'a') && (c <= 'f')) ? 10 + c - 'a' :
         ((c >= 'A') && (c <= 'F')) ? 10 + c - 'A' : 0;
}

// alternate method of debugging time from RTC
void printRTCTime(tmElements_t tm) {  
  printDigits(tm.Hour);
  Serial.print(F(":"));
  printDigits(tm.Minute);
  Serial.print(F(":"));
  printDigits(tm.Second);
  Serial.print(F(" "));
  printDigits(tm.Month);
  Serial.print(F("/"));
  printDigits(tm.Day);
  Serial.print(F("/"));
  Serial.print(tm.Year+1970);
  switch (tm.Wday) {
    case 1: Serial.println(F(" Sun"));
      break;
    case 2: Serial.println(F(" Mon"));
      break;
    case 3: Serial.println(F(" Tue"));
      break;
    case 4: Serial.println(F(" Wed"));
      break;
    case 5: Serial.println(F(" Thu"));
      break;
    case 6: Serial.println(F(" Fri"));
      break;
    case 7: Serial.println(F(" Sat"));
      break;
  }
}

void printLEDTime(tmElements_t tm) {
  uint8_t local_hr = ((24+tm.Hour+(dst*enableDST*1)-localOffset)%24);
  Serial.print(F("UTC.Hour:")); Serial.print(tm.Hour, DEC);
  Serial.print(F(" Local Hour:")); Serial.print(local_hr, DEC);
  local_hr = (local_hr*5)%60;  // Adjust hours based on DST
  Serial.print(F(" LED Time:"));
  printDigits(local_hr);
  Serial.print(F(":"));
  printDigits(tm.Minute);
  Serial.print(F(":"));
  printDigits(tm.Second);
  Serial.println();
  
}

void printDigits(int digits)
{
    // utility function for digital clock display: prints preceding colon and leading 0
    if(digits < 10)
        Serial.print(F("0"));
    Serial.print(digits);
}

String getRTCTime(tmElements_t tm) {  // format date time string for uart
  String st = "";
  st.concat((tm.Year+1970));
  st.concat('-');
  if (tm.Month < 10) st.concat('0');
  st.concat(tm.Month);
  st.concat('-');
  if (tm.Day < 10) st.concat('0');
  st.concat(tm.Day);
  st.concat('T');
  if (tm.Hour < 10) st.concat('0');
  st.concat(tm.Hour);
  st.concat(':');
  if (tm.Minute < 10) st.concat('0');
  st.concat(tm.Minute);
  st.concat(':');
  if (tm.Second < 10) st.concat('0');
  st.concat(tm.Second);
  // will need some check here to make sure we're not sending more than 20 bytes
  return st;
}

void add_color(uint8_t position, uint32_t color)
{
  uint32_t blended_color = blend(clock.getPixelColor (position), color);
  clock.setPixelColor(position, blended_color);
}
 
uint32_t blend(uint32_t color1, uint32_t color2)
{
  uint8_t r1,g1,b1;
  uint8_t r2,g2,b2;
 
  r1 = (uint8_t)(color1 >> 16),
  g1 = (uint8_t)(color1 >>  8),
  b1 = (uint8_t)(color1 >>  0);
 
  r2 = (uint8_t)(color2 >> 16),
  g2 = (uint8_t)(color2 >>  8),
  b2 = (uint8_t)(color2 >>  0);
 
  return clock.Color (constrain (r1+r2, 0, 255), constrain (g1+g2, 0, 255), constrain (b1+b2, 0, 255));
}

// Method to parse buffer string for alarm settings and call the alarm function
// String format $<num><enable><repeating><alarmType>:<second>:<minute>:<hour>:<daydate>
// Example: $2TT8E:00:00:11:01 - Set alarm 2 for every minute (Sec,Mn,Hr,Dy required but not used)
void parseAlarm(uint8_t *buffer) {
  uint8_t alarmNum;
  boolean enable, repeating;
  int at;
  ALARM_TYPES_t alarmType;
  byte sec, mn, hr, dydt;
  
  if (buffer[1] == '1') alarmNum = 1;
  else if (buffer[1] == '2') alarmNum = 2;
  else alarmNum = 0;
  if (toupper((char)buffer[2]) == 'T') enable = true;
  else enable = false;
  if (toupper((char)buffer[3]) == 'T') repeating = true;
  else repeating = false;
  at = (unhex(buffer[4]) << 4) + unhex(buffer[5]);
  sec = (unhex(buffer[7])*10) + unhex(buffer[8]);
  mn = (unhex(buffer[10])*10) + unhex(buffer[11]);
  hr = (unhex(buffer[13])*10) + unhex(buffer[14]);
  dydt = (unhex(buffer[16])*10) + unhex(buffer[17]);

  switch (at) {
    case ALM1_EVERY_SECOND: //ALM1_EVERY_SECOND = 0x0F
      alarmType = ALM1_EVERY_SECOND; break;
    case ALM1_MATCH_SECONDS: //ALM1_MATCH_SECONDS = 0x0E
      alarmType = ALM1_MATCH_SECONDS; break;
    case ALM1_MATCH_MINUTES: //ALM1_MATCH_MINUTES = 0x0C //match minutes *and* seconds
      alarmType = ALM1_MATCH_MINUTES; break;
    case ALM1_MATCH_HOURS: //ALM1_MATCH_HOURS = 0x08 //match hours *and* minutes, seconds
      alarmType = ALM1_MATCH_HOURS; break;
    case ALM1_MATCH_DATE: //ALM1_MATCH_DATE = 0x00 //match date *and* hours, minutes, seconds
      alarmType = ALM1_MATCH_DATE; break;
    case ALM1_MATCH_DAY: //ALM1_MATCH_DAY = 0x10 //match day *and* hours, minutes, seconds
      alarmType = ALM1_MATCH_DAY; break;
    case ALM2_EVERY_MINUTE: //ALM2_EVERY_MINUTE = 0x8E
      alarmType = ALM2_EVERY_MINUTE; break;
    case ALM2_MATCH_MINUTES: //ALM2_MATCH_MINUTES = 0x8C //match minutes
      alarmType = ALM2_MATCH_MINUTES; break;
    case ALM2_MATCH_HOURS: //ALM2_MATCH_HOURS = 0x88 //match hours *and* minutes
      alarmType = ALM2_MATCH_HOURS; break;
    case ALM2_MATCH_DATE: //ALM2_MATCH_DATE = 0x80 //match date *and* hours, minutes
      alarmType = ALM2_MATCH_DATE; break;
    case ALM2_MATCH_DAY: //ALM2_MATCH_DAY = 0x90 //match day *and* hours, minutes
      alarmType = ALM2_MATCH_DAY; break;
    default: break;
  }
  
  setAlarm(alarmNum, enable, repeating, alarmType, sec, mn, hr, dydt);
}

void setAlarm(uint8_t alarmNum, boolean enable, boolean repeating, ALARM_TYPES_t alarmType, byte seconds, byte minutes, byte hours, byte daydate) 
{
  switch (alarmNum) {
    case 1:
      // set alarm 1 for every hour at the specified minute
      RTC.setAlarm(alarmType, seconds, minutes, hours, daydate);
      //RTC.setAlarm(ALM1_MATCH_MINUTES, 00, 00, 1);
      alarmActive1 = enable;
      repeatAlarm1 = repeating;
      Serial.print(F("Alarm1 set, enb:"));
      Serial.print(alarmActive1);
      Serial.print(F("rpt:"));
      Serial.println(repeatAlarm1);
      break;
    case 2:
      // Set Alarm2 for every hour (Goes off when minutes = 00, ignores hour, minute and date)
      //RTC.setAlarm(ALM2_EVERY_MINUTE, 00, 00, 1);
      RTC.setAlarm(alarmType, seconds, minutes, hours, daydate);
      alarmActive2 = enable;
      repeatAlarm2 = repeating;
      Serial.print(F("Alarm2 set, enb:"));
      Serial.print(alarmActive2);
      Serial.print(F("rpt:"));
      Serial.println(repeatAlarm2);
      break;
    default:
      Serial.println(F("Wrong alarm number"));
      break;
  }
}

// borrowed from NeoPixel sample code
void rainbowCycle(uint8_t wait)
{
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< clock.numPixels(); i++) {
      clock.setPixelColor(i, Wheel(((i * 256 / clock.numPixels()) + j) & 255));
    }
    clock.show();
    delay(wait);
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return clock.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return clock.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return clock.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

boolean IsDST(uint8_t day, uint8_t month, uint8_t dow) {
  //January, february, and december are out.
  if (month < 3 || month > 11) { return false; }
  //April to October are in
  if (month > 3 && month < 11) { return true; }
  int previousSunday = day - dow + 1; // add 1 since dow is 1-7
  //In march, we are DST if our previous sunday was on or after the 8th.
  if (month == 3) { return previousSunday >= 8; }
  //In november we must be before the first sunday to be dst.
  //That means the previous sunday must be before the 1st.
  return previousSunday <= 0;
}

/*************************************************************************
    Configure the Arduino and start advertising with the radio
**************************************************************************/
void setup(void)
{ 
  Serial.begin(9600);
  while(!Serial); // Leonardo/Micro should wait for serial init
  Serial.println(F("Bluetooth clock demo"));

  uart.setRXcallback(rxCallback);
  uart.setACIcallback(aciCallback);
  uart.setDeviceName("Arduino"); /* 7 characters max! */
  uart.begin();
  
  clock.begin();
  clock.show(); // Initialize all pixels to 'off'
  
  Wire.begin();
  setSyncProvider(RTC.get);
  setSyncInterval(1);
  localOffset = 5;           // Eastern Time Zone
  prevSec = 0;
  
  if(timeStatus() != timeSet) {
    Serial.println(F("RTC is NOT running!"));
    // following line sets the RTC to the date & time this sketch was compiled
    // RTC.adjust(DateTime(__DATE__, __TIME__));
  }
  
  // disable alarm interrupt to avoid conflicts with SPI connection of Bluetooth LE module?
  //RTC.alarmInterrupt(ALARM_1, false);
  //RTC.alarmInterrupt(ALARM_2, false);
}

/*************************************************************************
    Constantly checks for new events on the nRF8001
**************************************************************************/
void loop()
{
  uart.pollACI();
  
  //utc = tmConvert_t(RTC.now()); // fetch the datetime
  //utc = RTC.get();
  //local = myTZ.toLocal(utc, &tcr); //Adjust UTC to local time zone
  //uint8_t hours = ((24+now.hour()+(dst*1)-localOffset)%24);  // Adjust hours based on DST
  
  unsigned long t = millis(); // Current elapsed time, milliseconds.
  // millis() comparisons are used rather than delay() so that animation
  // speed is consistent regardless of message length & other factors.
  
  aci_evt_opcode_t state = uart.getState();
 
  if(state != prevState) { // BTLE state change?
    switch(state) {        // Change LED flashing to show state
     //case ACI_EVT_DEVICE_STARTED: LEDperiod = 1000L / 10; break;
     case ACI_EVT_DEVICE_STARTED: LEDperiod = 1000L; break;
     case ACI_EVT_CONNECTED:      LEDperiod = 1000L / 10;  break;
     case ACI_EVT_DISCONNECTED:   LEDperiod = 0L;         break;
    }
    prevState   = state;
    prevLEDtime = t;
    LEDstate    = LOW; // Any state change resets LED
    digitalWrite(LED, LEDstate);
  }
 
  if(LEDperiod && ((t - prevLEDtime) >= LEDperiod)) { // Handle LED flash
    prevLEDtime = t;
    LEDstate    = !LEDstate;
    digitalWrite(LED, LEDstate);
  }
  tmElements_t tm;
  RTC.read(tm);
  // add function to check for DST. Need to update DS3232RTC library to support day of week read/write
  dst = IsDST(tm.Day, tm.Month, tm.Wday);
  uint8_t local_hr = ((24+tm.Hour+(dst*enableDST*1)-localOffset)%24);
  if (prevSec != tm.Second) {
    prevSec = tm.Second;
    printRTCTime(tm);
    //Serial.print(F("DST: ")); Serial.println(IsDST(tm.Day, tm.Month, tm.Wday)*enableDST);
    //Serial.println(getRTCTime(tm));
    //printLEDTime(tm);
  }
  
  // Display time on clock strip
  //uint8_t curr_hour = (hour()*5)%60; //UTC
  local_hr = (local_hr*5)%60;  // Adjust hours based on DST
  clock.clear();  // clear out previous pixel colors
  add_color(local_hr, hour_color);
  add_color(tm.Minute, minute_color);
  add_color(tm.Second, second_color);
  clock.show();
  
  // Check RTC alarms
  if ( RTC.alarm(ALARM_2) ) {     //has Alarm2 triggered?
      //yes, act on the alarm
      if (alarmActive2) {
        Serial.println(F("Alarm 2!"));
        colorWipe(local_hr); // Warning: Blocking method
        //flashAlarm(0x200000, false);
        if (!repeatAlarm2) alarmActive2 = false; // disable alarm if not repeating
      } else {
        // alarm not active
        Serial.println(F("A2 not active"));
      }
    }
    else if ( RTC.alarm(ALARM_1) ) {     //has Alarm1 triggered?
      //yes, act on the alarm
      if (alarmActive1) {
        Serial.println(F("Alarm 1!"));
        colorWipe(local_hr); // Warning: Blocking method
        //flashAlarm(0x20, true);
        if (!repeatAlarm1) alarmActive1 = false; // disable alarm if not repeating
      } else {
        // alarm not active
        Serial.println(F("A1 not active"));
      }
    }
    else {
      //no alarm
    }
}
