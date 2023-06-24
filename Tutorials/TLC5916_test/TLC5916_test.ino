/**************************************************************************
       Title:   TLC5916 Test              
      Author:   Bruce E. Hall, w8bh.net
        Date:   04 May 2023
    Hardware:   ESP8266 Wemos D1 mini, TLC5916, 8 LEDs
    Software:   Arduino IDE 1.8.19
       Legal:   Copyright (c) 2023  Bruce E. Hall.
                Open Source under the terms of the MIT License. 
    
 Description:   Testing the TLC5916 constant current driver
                Connect it to 8 LEDs for cylon-type display.
 
                TLC5916 Pin Connections:
                1   GND                16  VCC (to 3.3V)
                2   SDI (to MOSI)      15  Rext (to 4.7K for 4mA OUTPUT)
                3   CLK (to SCK)       14  SDO (no connection)
                4   LE  (to LATCH)     13  OE (to GND)
                5   OUTPUT0            12  OUTPUT7
                6   OUTPUT1            11  OUTPUT6
                7   OUTPUT2            10  OUTPUT5 
                8   OUTPUT3             9  OUTPUT4
                
                All OUTPUTs connected directly to a LED cathode.
                All LED anodes connected directly to Vcc (3.3V)

 **************************************************************************/

#define MOSI    13            // On D1 mini, D7
#define SCK     14            // on D1 mini, D5
#define LATCH   15            // on D1 mini, D8

#include "SPI.h"

int pattern = 0b00001111;     // LED pattern to rotate

void writeByte(byte b) {
  SPI.transfer(b);            // send 1 byte via SPI
  digitalWrite(LATCH,HIGH);   // latch it on upgoing pulse
  digitalWrite(LATCH,LOW);    // complete latch pulse
}


void setup() {
  pinMode(LATCH,OUTPUT);
  pinMode(MOSI, OUTPUT);
  pinMode(SCK,  OUTPUT);
  digitalWrite(LATCH,LOW);   // start with latch low
  SPI.begin();               // using SPI for data transfer
}

void loop() {
  writeByte(pattern);        // display the pattern
  pattern <<= 1;             // shift it left 1 position
  if (pattern>255)           // if result carried to 'bit8'
    pattern-=255;            // set bit0 instead
  delay(100);                // do 10 changes per second
}
