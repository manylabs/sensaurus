#ifndef _CHECK_STREAM_H_
#define _CHECK_STREAM_H_
#include "Arduino.h"


// this library is based on code from Modular Science


#define CHECK_STREAM_BUF_LEN 100


uint16_t crc16_update(uint16_t crc, uint16_t data);


class CheckStream : public Stream {
public:

	CheckStream(Stream &stream) : m_stream(stream) {
		m_crc = 0xffff;
		#ifdef USE USE_CHECK_STREAM_BUFFER
			m_bufferIndex = 0;
		#endif
	}

	size_t write(byte data) {
		if (data == 13) {  // assume we get a 13, 10 at end of line
			#ifdef USE_CHECK_STREAM_BUFFER
				m_buffer[m_bufferIndex] = 0;
				m_bufferIndex = 0;
				m_stream.print(m_buffer);
			#endif
			m_stream.print('|');
			m_stream.print(m_crc, HEX);
			m_stream.println();
			m_crc = 0xffff;
			return 1;
		} else if (data == 10) {  // ignore this, since we've already sent a newline above
			return 1;
		} else {
			m_crc = crc16_update(m_crc, data);
			#ifdef USE_CHECK_STREAM_BUFFER
				if (m_bufferIndex < CHECK_STREAM_BUF_LEN - 1) {
					m_buffer[m_bufferIndex] = (char) data;
					m_bufferIndex++;
				}
				return 1;
			#else
				return m_stream.write(data);
			#endif
		}
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
	#ifdef USE_CHECK_STREAM_BUFFER
		int m_bufferIndex;
		char m_buffer[CHECK_STREAM_BUF_LEN];
	#endif
};


#endif  // _CHECK_STREAM_H_
