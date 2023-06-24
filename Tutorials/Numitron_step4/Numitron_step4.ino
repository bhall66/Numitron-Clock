/**************************************************************************
       Title:   Numitron Step4: Single-Tube clock  
      Author:   Bruce E. Hall, w8bh.net
        Date:   22 Jun 2023
    Hardware:   ESP8266 Wemos D1 mini, TLC5916, DR2200 Numitron
    Software:   Arduino IDE 2.1.0 with Expressif ESP32 package 
                ezTime Library
       Legal:   Copyright (c) 2023  Bruce E. Hall.
                Open Source under the terms of the MIT License. 
                    
 Description:   NTP Clock with single Numitron display 
                Time is refreshed via NTP every 30 minutes
                Optional time output to serial port 

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

                This code works with BN57:  Connect D5/D7/D8/GND/3.3V
               
 **************************************************************************/

#include "SPI.h"
#include <ezTime.h>                              // https://github.com/ropg/ezTime
#include <ESP8266WiFi.h>                         // use this WiFi lib for ESP8266
                  
#define LATCH               D8            
#define WIFI_SSID           "Your SSID"               
#define WIFI_PWD            "Your PWD"      
#define NTP_SERVER          "192.168.86.60"      // time.nist.gov, pool.ntp.org, etc      
#define TZ_RULE             "EST5EDT,M3.2.0/2:00:00,M11.1.0/2:00:00"                                                  
#define FORMAT_12HR         true                 // time format 12hr "11:34" vs 24hr "23:34"
#define DISPLAY_AMPM        true                 // "10:06P" vs "10:06"

// ============ GLOBAL VARIABLES =====================================================

Timezone local;                                  // local timezone variable

const byte segments[] = 
  {0xE7, 0x41, 0xB3, 0xF1, 0x55,                 // segments for digits 0..4
  0xF4, 0xF6, 0x61, 0xF7, 0xF5};                 // segments for digits 5..9

// ============ DISPLAY ROUTINES =====================================================

void writeByte(byte b) {
  SPI.transfer(b);                               // send 1 byte via SPI
  digitalWrite(LATCH,HIGH);                      // latch it on upgoing pulse
  digitalWrite(LATCH,LOW);                       // complete latch pulse
}

void displayDigit(int digit) {                   // display a number 0..9 on the Numitron
    writeByte(segments[digit]);                  // send segment data to shift register
}

void displayOff() {                              // turn off the Numitron display
  writeByte(0);                                  // by setting all segments to 0=off
}

void pause() {                                   // half-second delay, consisting of
  delay(460);                                    // holding the display for 460 mS
  displayOff();                                  // then briefly turning display off 
  delay(40);                                     // to resolve double digits like "22" 
}

void showTime(time_t t) {
  int h = hour(t);                               // get the current hour
  int m = minute(t);                             // and the current minute
  if (FORMAT_12HR) {                             // adjust to 12-hour format?
    if (h==0) h=12;                              // 00:00 becomes 12:00
    if (h>12) h-=12;                             // 13:00 becomes 01:00
   }
  displayDigit(h/10);  pause();                  // show hours as 2 digits
  displayDigit(h%10);  pause();                         
  displayDigit(m/10);  pause();                  // show minutes as 2 digits
  displayDigit(m%10);  pause(); 
  if (FORMAT_12HR && DISPLAY_AMPM) {             // add AM/PM indicator for 12-hr time?
    if (hour(t)<12) writeByte(0b01110111);       // display 'A' for AM
    else writeByte(0b00110111);                  // display 'P' for PM  
    pause();            
  }                         
}


// ============ MAIN PROGRAM ===================================================

void setup() {
  pinMode(LATCH,OUTPUT);
  digitalWrite(LATCH,LOW);                       // start with latch low
  SPI.begin();                                   // using SPI for data transfer
  displayOff();                                  // show sketch is starting
  setServer(NTP_SERVER);                         // set NTP server
  WiFi.begin(WIFI_SSID, WIFI_PWD);               // start WiFi
  waitForSync();                                 // get time info from internet
  local.setPosix(TZ_RULE);                       // estab. local TZ by rule 
}

void loop() {
  events();                                      // get periodic NTP updates
  showTime(local.now());                         // display the local time
  displayOff();                                  // then turn display off
  delay(1000);                                   // for a second, before repeating
}