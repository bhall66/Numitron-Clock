/**************************************************************************
       Title:   Numitron six-digit clock
      Author:   Bruce E. Hall, w8bh.net
        Date:   23 Jun 2023
    Hardware:   ESP8266 Wemos D1 mini, TLC5916/DR2200 Numitron x 6
    Software:   Arduino IDE 2.1.0 with Expressif ESP32 package 
                ezTime, WifiManager, ESP8266WiFi libraries
       Legal:   Copyright (c) 2023  Bruce E. Hall.
                Open Source under the terms of the MIT License. 
                    
 Description:   NTP Clock with 6-digit Numitron display 
                Time is refreshed via NTP every 30 minutes
                Optional time output to serial port 
                
                Works with BN58 & BN59 circuit boards.
                This version does not yet support DS3231 RTC backup.

                Before using, modify TZ_RULE with your own Posix 
                timezone string.
               
 **************************************************************************/

#include "SPI.h"
#include <ezTime.h>                              // https://github.com/ropg/ezTime
#include <WiFiManager.h>                         // https://github.com/tzapu/WiFiManager
#include <ESP8266WiFi.h>                         // use this WiFi lib for ESP8266

#define OE                      D3               // TLC916 output enable, active low
#define LED_PIN                 D4               // built-in LED on GPIO 2
#define SCK                     D5               // SPI clock on GPIO 14
#define MOSI                    D7               // SPI data on GPIO 13  
#define LATCH                   D8               // TLC5916 data latch on GPIO 15 
#define SWITCH                  A0               // user pushbutton

#define TITLE              "NUMITRON clock"    
#define NTP_SERVER         "pool.ntp.org"        // time.nist.gov, pool.ntp.org, etc    
#define TZ_RULE            "EST5EDT,M3.2.0/2:00:00,M11.1.0/2:00:00"

#define DEBUGLEVEL            INFO               // NONE, ERROR, INFO, or DEBUG
#define PRINTED_TIME             2               // 0=NONE, 1=UTC, or 2=LOCAL
#define TIME_FORMAT         COOKIE               // COOKIE, ISO8601, RFC822, RFC850, RFC3339, RSS
#define BAUDRATE            115200               // serial output baudrate                                       
#define LOCAL_FORMAT_12HR     true               // local time format 12hr "11:34" vs 24hr "23:34"
#define UTC_FORMAT_12HR      false               // UTC time format 12 hr "11:34" vs 24hr "23:34"
#define HOUR_LEADING_ZERO    false               // "01:00" vs " 1:00" (only applies to 12hr mode)
#define BRIGHTNESS_LEVEL        10               // 0=off, 1=very dim, ..., 10=full brightness


// ============ GLOBAL VARIABLES =====================================================

Timezone local;                                  // local timezone variable
WiFiManager wifiManager;                         // Manager only used in this routine
bool useLocalTime  = true;                       // start clock in local time
byte digit[6];                                   // scratchpad memory for display digits 0-5

const char knownChars[] =                        // chars that can be shown on a 7-segment display
  "0123456789AbCcdEFGHhIiJLlNnOoPqrStUuyZ _-=?[]'\"*";    

// the following array defines which Numitron display segments are lit for a given character.
// Each bit corresponds an individual segment in the order "dcagxfeb", MSB to LSB.
// For example, the value "0b10000000" specifies that segment "d" is lit.  This is the bottom segment
// of the display and therefore generates the underscore character.  The character order of this array
// follows the "knownChars" string, specified avove.   

const byte segData[] = {                           
//   dcagxfeb   7-segment map, with x as decimal point
   0b11100111,  // 0 
   0b01000001,  // 1
   0b10110011,  // 2
   0b11110001,  // 3
   0b01010101,  // 4
   0b11110100,  // 5
   0b11110110,  // 6
   0b01100001,  // 7
   0b11110111,  // 8
   0b11110101,  // 9
   0b01110111,  // A
   0b11010110,  // b
   0b10100110,  // C  
   0b10010010,  // c
   0b11010011,  // d
   0b10110110,  // E
   0b00110110,  // F
   0b11100110,  // G
   0b01010111,  // H
   0b01010110,  // h
   0b00000110,  // I
   0b01000000,  // i
   0b11000001,  // J
   0b10000110,  // L
   0b10000010,  // l
   0b01100111,  // N
   0b01010010,  // n
   0b11100111,  // O
   0b11010010,  // o
   0b00110111,  // P
   0b01110101,  // q
   0b00010010,  // r
   0b11110100,  // S
   0b10010110,  // t
   0b11000111,  // U
   0b11000010,  // u
   0b11010101,  // y
   0b10110011,  // Z
   0b00000000,  // (space)
   0b10000000,  // _
   0b00010000,  // -
   0b10010000,  // =
   0b00110011,  // ?
   0b10100110,  // [
   0b11100001,  // ]
   0b00000100,  // '
   0b00000101,  // "
   0b00110101,  // * (degree)
   0b00000000   // (not found)
};


// ============ DISPLAY ROUTINES =====================================================

byte alpha(char c) {                             // return segment data for given character
  int i = 0;                                     // character search index
  while ((i<strlen(knownChars)) &&               // search all known characters for c
    (c!=knownChars[i]))                          // compare c with each known character. 
    i++;                                         // no match, so look at next character
  return segData[i];                             // when match found, return its segment data
}

void writeDisplay() {
  for (int i=6; i>=0; --i)                       // update digits 0-5 in reverse order
    SPI.transfer(digit[i]);                      // send data to display via SPI, 1 byte/digit
  digitalWrite(LATCH,HIGH);                      // latch it on upgoing pulse
  digitalWrite(LATCH,LOW);                       // complete latch pulse  
}

void enableDisplay(bool on=true) {               // false = display disabled (off)
  digitalWrite(OE,on?0:1);                       // OE = TLC5916 output enable, active low
}

void setBrightness(int level=10) {               // scale: 0=off, 10=full brightness
  const byte data[] = {255, 172, 160, 140, 
    120,100, 80, 60, 40, 20, 0};
  if ((level>=0)&&(level<=10))                   // keep array index in bounds
    analogWrite(OE,data[level]);                 // do PWM on the OE pin 
}

bool mode12h() {                                 // return true is clock is in 12-hr mode              
  return (useLocalTime && LOCAL_FORMAT_12HR)     // 12-hr format for local time
  || (!useLocalTime && UTC_FORMAT_12HR);         // 12-hr format for UTC time
}

void showTime(time_t t) {                        // display time as "HH MM SS"
  int h = hour(t);                               // get hours, minutes, and seconds
  int m = minute(t);
  int s = second(t);
  if (mode12h()) {                               // if using 12-hour format:                                        
    if (h==0) h=12;                              // 00:00 becomes 12:00
    if (h>12) h-=12;                             // 13:00 becomes 01:00
  }
  if ((h<10) && !HOUR_LEADING_ZERO               // suppress a leading zero?
  && mode12h())                                  // (only in 12hr mode)                                   
    digit[0] = 0;                                // yes, blank it
  else digit[0] = segData[h/10];                 // hours 1st digit
  digit[1] = segData[h%10];                      // hours 2nd digit
  digit[2] = segData[m/10];                      // minutes 1st digit
  digit[3] = segData[m%10];                      // minutes 2nd digit
  digit[4] = segData[s/10];                      // seconds 1st digit
  digit[5] = segData[s%10];                      // seconds 2nd digit
  writeDisplay();                                // send data to display
}

void showDate(time_t t) {                        // display date as "MM DD YY"
  int m = month(t);                              // get months, days, and years
  int d = day(t);
  int y = year(t)-2000;                          // convert to 2-digit year (2023->23)
  digit[0] = segData[m/10];                      // months 1st digit
  digit[1] = segData[m%10];                      // months 2nd digit
  digit[2] = segData[d/10];                      // days 1st digit
  digit[3] = segData[d%10];                      // days 2nd digit
  digit[4] = segData[y/10];                      // minutes 1st digit
  digit[5] = segData[y%10];                      // minutes 2nd digit
  writeDisplay();                                // send data to display
}

void showString(const char *str) {
  for (int i=0; i<strlen(str); i++)              // for each character in string..
    digit[i] = alpha(str[i]);                    // get segment data for that character
  writeDisplay();                                // send data to display
}

void startupScreen() { 
  if (switchPressed()) return;                   // bypass startup if switch is pressed
  for (int i=0; i<9; i++) {                      // count 0-8 on all digits
    for (int d=0; d<6; d++)                      // for each digit #0-#5
      digit[d] = segData[i];                     // set digit to the current count
    writeDisplay();                              // send data to display
    delay(150);                                  // wait for all segments to illuminate
  }
  for (int i=0; i<4; i++) {                      // flash 8 on all digits
    enableDisplay(false); delay(250);            // display disabled                               
    enableDisplay(true); delay(750);             // display enabled
  }
  delay(500);
  showString("brucE "); delay(2000);             // display "Bruce Hall"
  showString("  HALL"); delay(2000);
}


// ============ WiFi ROUTINES =====================================================

void APCallback (WiFiManager *myWiFiManager) {   // WiFi network cannot be accessed,
  showString("  CELL");                          // so notify user
}                                                // to enter good WiFi credentials

void wifiConnect() {
  Serial.println("Connecting to WiFi");
  if (switchPressed())                           // is user pressing reset switch?    
    wifiManager.resetSettings();                 // if so, erase current WiFi credentials
  wifiManager.setAPCallback(APCallback);
  wifiManager.setConfigPortalTimeout(180);       // AP open 3 minutes to enter Wifi info
  wifiManager.setDebugOutput(false);             // dont print WiFi debug info
  if(!wifiManager.autoConnect(TITLE)) {          // create an access point for creds.
    showString("no nEt");                        // user didn't enter WiFi credentials
    delay(3000);                                 // allow time to close access point
    ESP.restart();                               // let's start again, shall we?
    delay(5000);
  } 
}

void checkWiFi() {                               // periodically check WiFi status
  if (WiFi.status()!=WL_CONNECTED) {             // and reconnect when necessary
    showString("no nEt");                        // display network connectivity issue 
    wifiConnect();                               // try to reconnect     
  }   
  delay(50);                                     // go too fast & you'll lose WiFi (bug)          
}


// ============ MISC ROUTINES =====================================================

bool switchPressed() {
  return (analogRead(SWITCH)>250);               // check for voltage on switched pin
}

void updateDisplay() {
  static int t=0;
  if (t!=now()) {                                // are we in a new second yet?
    t = now();                                   // yes, so remember it                               
    if (useLocalTime) showTime(local.now());     // display time in local timezone
    else showTime(t);                            // display time  in UTC
    printTime();                                 // send time to serial monitor
    checkWiFi();                                 // periodically make sure WiFi is connected.
  }
}

void handleSwitch() {
  showString("  dAtE");                          // assume user wants to see the date
  delay(1000);                                   // allow user to release switch
  if (!switchPressed()) {                        // is switch still held down?
    if (useLocalTime) showDate(local.now());     // no, so show the date (local time)
    else showDate(now());                        //  vs. utc time
    delay(3000);                                 // ... for 3 seconds.
  } else {                                       // user held down the switch, so
    useLocalTime = !useLocalTime;                // toggle the utc/local time flag
    if (useLocalTime) showString(" LOCAL");      // and display flag value
    else showString("   UtC");
    delay(2000);
  } 
}

void printTime() {                               // print time to serial port
  if (PRINTED_TIME==0) return;                   // option 0: dont print
  Serial.print("TIME: ");
  if (PRINTED_TIME==1)
    Serial.println(dateTime(TIME_FORMAT));       // option 1: print UTC time
  else 
    Serial.println(local.dateTime(TIME_FORMAT)); // option 2: print local time
}

void blink(int count=1) {                        // diagnostic LED blink
  pinMode(LED_PIN,OUTPUT);                       // make sure pin is an output
  for (int i=0; i<count; i++) {                  // blink counter
    digitalWrite(LED_PIN,0);                     // turn LED on 
    delay(200);                                  // for 0.2s
    digitalWrite(LED_PIN,1);                     // and then turn LED off
    delay(200);                                  // for 0.2s
  } 
  pinMode(LED_PIN,INPUT);                        // works for both Vcc & Gnd LEDs.
}


// ============ MAIN PROGRAM ===================================================

void setup() {
  pinMode(OE, OUTPUT);                           // TLC5916 output enable pin
  pinMode(SWITCH, INPUT);                        // input for user pushbutton
  pinMode(LATCH, OUTPUT);                        // TLC5916 data latch
  pinMode(MOSI, OUTPUT);                         // SPI data output
  pinMode(SCK,  OUTPUT);                         // SPI clock output
  digitalWrite(LATCH,LOW);                       // start with latch pin low
  SPI.begin();                                   // using SPI for data transfer
  enableDisplay(false);                          // dont show garbage on power-up
  blink(3);                                      // show sketch is starting
  enableDisplay();                               // turn on Numitron display
  startupScreen();                               // display all segments to user
  setBrightness(BRIGHTNESS_LEVEL);               // adjust display brightness
  Serial.begin(BAUDRATE);                        // open serial port
  Serial.println(TITLE);
  setDebug(DEBUGLEVEL);                          // enable NTP debug info
  setServer(NTP_SERVER);                         // set NTP server
  setInterval(600);
  wifiConnect();                                 // establish WiFi connection                          
  local.setPosix(TZ_RULE);                       // estab. local TZ by rule 
}

void loop() {
  events();                                      // get periodic NTP updates
  if (switchPressed()) handleSwitch();           // check for user switch presses
  updateDisplay();                               // update clock every second
}