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

#include <HubSerial.h>

#define MAX_PIN 35

HubSerial::HubSerial(int pin, unsigned int buffSize) {
   m_pin = pin;
   m_readMode = true;
   m_buffSize = buffSize;
   m_buffer = (uint8_t*)malloc(m_buffSize);
   m_inPos = m_outPos = 0;
   m_overflow = false;
}

HubSerial::~HubSerial() {
   if (m_buffer)
      free(m_buffer);
}

void HubSerial::begin(long speed) {
   // Use getCycleCount() loop to get as exact timing as possible
   m_bitTime = ESP.getCpuFreqMHz()*1000000/speed;
   pinMode(m_pin, INPUT);
   m_readMode = true;
}

int HubSerial::read() {
   if (m_inPos == m_outPos) return -1;
   uint8_t ch = m_buffer[m_outPos];
   m_outPos = (m_outPos+1) % m_buffSize;
   return ch;
}

int HubSerial::available() {
   int avail = m_inPos - m_outPos;
   if (avail < 0) avail += m_buffSize;
   return avail;
}

void HubSerial::flush() {
   m_inPos = m_outPos = 0;
}

int HubSerial::peek() {
   if (m_inPos == m_outPos) return -1;
   return m_buffer[m_outPos];
}


#define WAIT { while (ESP.getCycleCount()-start < wait); wait += m_bitTime; }

size_t HubSerial::write(uint8_t b) {

   // if needed, switch from reading to writing
   if (m_readMode) {
      pinMode(m_pin, OUTPUT);
      m_readMode = false;
   }

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


void HubSerial::busyReadByte(unsigned long timeout) {

   // if needed, switch from writing to reading
   if (m_readMode == false) {
      pinMode(m_pin, INPUT);
      m_readMode = true;
   }

   // Wait for start bit
   unsigned long startTime = millis();
   while (digitalRead(m_pin)) {
     if (millis() - startTime > timeout) {
        return;
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
   // Store the received value in the buffer unless we have an overflow
   int next = (m_inPos+1) % m_buffSize;
   if (next != m_outPos) {
      m_buffer[m_inPos] = rec;
      m_inPos = next;
   } else {
      m_overflow = true;
   }
}
