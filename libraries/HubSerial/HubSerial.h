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

#ifndef HubSerial_h
#define HubSerial_h

#include <inttypes.h>
#include <Stream.h>


class HubSerial : public Stream
{
public:
   HubSerial(int pin, unsigned int buffSize = 64);
   ~HubSerial();

   void begin(long speed);

   virtual size_t write(uint8_t byte);
   virtual int read();
   virtual int available();
   int peek();
   virtual void flush();

   void busyReadByte(unsigned long timeout);

   using Print::write;

private:

   int m_pin;
   bool m_readMode;
   unsigned long m_bitTime;
   unsigned int m_inPos, m_outPos;
   int m_buffSize;
   uint8_t *m_buffer;
   bool m_overflow;

};


#endif
