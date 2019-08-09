/*

Based on https://github.com/akshaybaweja/SoftwareSerial, which is based on code with the following license:

SoftwareSerial.cpp - Implementation of the Arduino software serial for ESP8266.
Copyright (c) 2015-2016 Peter Lerup. All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include <Arduino.h>

// The Arduino standard GPIO routines are not enough,
// must use some from the Espressif SDK as well
extern "C" {
    #include "esp32-hal-gpio.h"
}

#include <SimpleHubSerial.h>

#define MAX_PIN 35

SimpleHubSerial::SimpleHubSerial(int pin) {
   m_pin = pin;
}

void SimpleHubSerial::begin(long speed) {
   // Use getCycleCount() loop to get as exact timing as possible
   m_bitTime = ESP.getCpuFreqMHz()*1000000/speed;
   pinMode(m_pin, OUTPUT);
   digitalWrite(m_pin, HIGH);
}

#define WAIT { while (ESP.getCycleCount()-start < wait); wait += m_bitTime; }

size_t SimpleHubSerial::write(uint8_t b) {

   // Disable interrupts in order to get a clean transmit
   cli();
   unsigned long wait = m_bitTime;
   digitalWrite(m_pin, HIGH);
   unsigned long start = ESP.getCycleCount();
    // Start bit;
   digitalWrite(m_pin, LOW);
   WAIT;
   for (int i = 0; i < 8; i++) {
     digitalWrite(m_pin, (b & 1) ? HIGH : LOW);
     WAIT;
     b >>= 1;
   }
   // Stop bit
   digitalWrite(m_pin, HIGH);
   WAIT;
   sei();
   return 1;
}


byte SimpleHubSerial::readByte(unsigned long timeout) {

   // Wait for start bit
   unsigned long startTime = millis();
   while (digitalRead(m_pin)) {
     if (millis() - startTime > timeout) {
        return 0;
     }
   }

   // Advance the starting point for the samples but compensate for the
   // initial delay which occurs before the interrupt is delivered
   unsigned long wait = m_bitTime + m_bitTime/3 - 500;
   unsigned long start = ESP.getCycleCount();
   uint8_t rec = 0;
   for (int i = 0; i < 8; i++) {
     WAIT;
     rec >>= 1;
     if (digitalRead(m_pin))
       rec |= 0x80;
   }
   // Stop bit
   WAIT;
   return rec;
}
