#ifndef _CHECK_STREAM_H_
#define _CHECK_STREAM_H_
#include "Arduino.h"


// this library is based on code from Modular Science


uint16_t crc16_update(uint16_t crc, uint16_t data);


class CheckStream : public Stream {
public:

	CheckStream(Stream &stream) : m_stream(stream) {
		m_crc = 0xffff;
		m_empty = true;
	}

	size_t write(byte data) {
		if (data == 10 || data == 13) {
			if (m_empty == false) {  // don't want to add a checksum to an empty message
				m_stream.print('|');
				m_stream.print(m_crc, HEX);
			}
			m_empty = true;  // reset
			m_crc = 0xffff;
		} else {
			m_crc = crc16_update(m_crc, data);
			m_empty = false;
		}
		return m_stream.write(data);
	}

	// direct pass-through to stream class
	void flush() { m_stream.flush(); }
	int read() { return m_stream.read(); } 
	int available() { return m_stream.available(); }
	int peek() { return m_stream.peek(); }

private:

	// internal data
	Stream &m_stream;
	uint16_t m_crc;
	bool m_empty;
};


#endif  // _CHECK_STREAM_H_
