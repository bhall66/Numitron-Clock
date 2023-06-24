/**************************************************************************
       Title:   Numitron Step2: Brightness Control              
      Author:   Bruce E. Hall, w8bh.net
        Date:   23 June 2023
    Hardware:   ESP8266 Wemos D1 mini, TLC5916, Numitron
    Software:   Arduino IDE 1.8.19
       Legal:   Copyright (c) 2023  Bruce E. Hall.
                Open Source under the terms of the MIT License. 
    
 Description:   Testing Numitron DR2100V1 with TLC5916 constant current driver
                This test will use PWM to control Numitron brightness

                Use with breakout board BN57, or wire as below.
 
                TLC5916 Pin Connections:
                1   GND                16  VCC (to 3.3V)
                2   SDI (to MCU D7)    15  Rext (to 1.5K for 12.5mA OUTPUT)
                3   CLK (to MCU D5)    14  SDO (no connection)
                4   LE  (to MCU D8)    13  OE (to MCU D3)
                5   Numitron_Pin8      12  Numitron_Pin4
                6   Numitron_Pin3      11  Numitron_Pin5
                7   Numitron_Pin9      10  Numitron_Pin7
                8   Numitron_Pin1       9  Numitron_Pin6
                
                Numitron_Pin2 connected directly to Vcc (3.3V)
                With these connections, data bits #7-#0 are mapped to
                the numitron segments as follows:  "dcagxfeb"

 **************************************************************************/
           
#include "SPI.h"              // Serial Peripheral Interface

#define LATCH     D8          // to TLC5916 LE (latch enable) pin
#define OE        D3          // to TLC5916 OE (output enable) pin

void writeByte(byte b) {
  SPI.transfer(b);            // send 1 byte to shift register via SPI
  digitalWrite(LATCH,HIGH);   // latch it on upgoing pulse
  digitalWrite(LATCH,LOW);    // complete latch pulse
}

void setup() {
  pinMode(OE,OUTPUT);         // use OE for brightness control
  pinMode(LATCH,OUTPUT);
  digitalWrite(LATCH,LOW);    // start with latch low
  SPI.begin();                // using SPI for data transfer
  writeByte(0xFF);            // turn on all Numitron segments "8" 
}

void loop() {
  for (int brightness = 0; brightness < 255; brightness++) {
    analogWrite(OE, brightness);
    delay(10);
  }
  for (int brightness = 255; brightness >= 0; brightness--) {
    analogWrite(OE, brightness);
    delay(10);
  }
}
