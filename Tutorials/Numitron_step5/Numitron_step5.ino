/**************************************************************************
       Title:   Numitron Step5:  simple six-digit clock 
      Author:   Bruce E. Hall, w8bh.net
        Date:   22 Jun 2023
    Hardware:   ESP8266 Wemos D1 mini, TLC5916, DR2200 Numitron
    Software:   Arduino IDE 2.1.0 with Expressif ESP32 package 
                ezTime Library
       Legal:   Copyright (c) 2023  Bruce E. Hall.
                Open Source under the terms of the MIT License. 
                    
 Description:   NTP Clock with six Numitron displays 
                Time is refreshed via NTP every 30 minutes

                Before using, please update WIFI_SSID and WIFI_PWD
                with your personal WiFi credentials.  Also, modify
                TZ_RULE with your own Posix timezone string.

                TLC5916 Pin Connections.  Outputs -> Numitron segment
                1   GND                16  VCC (to 3.3V)
                2   SDI (to MOSI)      15  Rext (to 1.5K for 17mA OUTPUT)
                3   CLK (to SCK)       14  SDO (no connection)
                4   LE  (to LATCH)     13  OE (to GND)
                5   OUTPUT0 -> b       12  OUTPUT7 -> d
                6   OUTPUT1 -> e       11  OUTPUT6 -> c
                7   OUTPUT2 -> f       10  OUTPUT5 -> a
                8   OUTPUT3 -> nc       9  OUTPUT4 -> g
                
                All OUTPUTs connected directly to LED cathode.
                All LED anodes connected directly to Vcc (3.3V)

                This code works with boards BN58 & BN59.               
 **************************************************************************/

#include "SPI.h"
#include <ezTime.h>                              // https://github.com/ropg/ezTime
#include <ESP8266WiFi.h>                         // use this WiFi lib for ESP8266
                  
#define OE                  D3
#define LATCH               D8            
#define WIFI_SSID           "Your SSID"               
#define WIFI_PWD            "Your PWD"      
#define NTP_SERVER          "192.168.86.60"      // time.nist.gov, pool.ntp.org, etc      
#define TZ_RULE             "EST5EDT,M3.2.0/2:00:00,M11.1.0/2:00:00"                                                  
#define FORMAT_12HR         true                 // time format 12hr "11:34" vs 24hr "23:34"

// ============ GLOBAL VARIABLES =====================================================

Timezone local;                                  // local timezone variable
byte tube[6];                                    // scratchpad memory for 6 display tubes

const byte segments[] = 
  {0xE7, 0x41, 0xB3, 0xF1, 0x55,                 // segments for digits 0..4
  0xF4, 0xF6, 0x61, 0xF7, 0xF5};                 // segments for digits 5..9

// ============ DISPLAY ROUTINES =====================================================

void enableDisplay(bool on=true) {               // false = display disabled (off)
  digitalWrite(OE,on?0:1);                       // OE = TLC5916 output enable, active low
}

void writeDisplay() {
  for (int i=6; i>=0; --i)                       // update digits 0-5 in reverse order
    SPI.transfer(tube[i]);                       // send data to display, 1 byte/tube
  digitalWrite(LATCH,HIGH);                      // latch it on upgoing pulse
  digitalWrite(LATCH,LOW);                       // complete latch pulse
}

void showTime(time_t t) {
  int h = hour(t);                               // get the current hour
  int m = minute(t);                             // and the current minute
  int s = second(t);                             // and the current seconds
  if (FORMAT_12HR) {                             // adjust to 12-hour format?
    if (h==0) h=12;                              // 00:00 becomes 12:00
    if (h>12) h-=12;                             // 13:00 becomes 01:00
   }
  tube[0] = segments[h/10];                      // hours 1st digit
  tube[1] = segments[h%10];                      // hours 2nd digit
  tube[2] = segments[m/10];                      // minutes 1st digit
  tube[3] = segments[m%10];                      // minutes 2nd digit
  tube[4] = segments[s/10];                      // seconds 1st digit
  tube[5] = segments[s%10];                      // seconds 2nd digit
  writeDisplay();                                // send data to display                 
}


// ============ MAIN PROGRAM ===================================================

void setup() {
  pinMode(OE,OUTPUT);
  pinMode(LATCH,OUTPUT);
  digitalWrite(LATCH,LOW);                       // start with latch low
  SPI.begin();                                   // using SPI for data transfer
  setServer(NTP_SERVER);                         // set NTP server
  WiFi.begin(WIFI_SSID, WIFI_PWD);               // start WiFi
  waitForSync();                                 // get time info from internet
  local.setPosix(TZ_RULE);                       // estab. local TZ by rule 
  enableDisplay();                               // force OE low; turns on display                    
}

void loop() {
  events();                                      // get periodic NTP updates
  if (secondChanged())                           // when a new second occurs,
    showTime(local.now());                       // show local time on display
}