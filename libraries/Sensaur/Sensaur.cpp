#include "Sensaur.h"
#include "EEPROM.h"
#include "CheckStream.h"


// parse a message sent using the sensaur serial protocol
// returns number of args or -1 if error (e.g. checksum error)
// note: parts of the message will get overwritten during parsing
byte parseMessage(char *message, char **command, char *args[], byte maxArgs, char separator) {
	*command = message;
	int pos = 0;
	int argCount = 0;
	int crc = 0xffff;
	while (message[pos]) {
		char c = message[pos];
		if (c == ':') {
			message[pos] = 0;
			if (maxArgs) {
				args[0] = message + pos + 1;
			}
			argCount++;
		} else if (c == separator) {
			message[pos] = 0;
			if (argCount < maxArgs) {
				args[argCount] = message + pos + 1;
			}
			argCount++;
		} else if (c == '|') {
			message[pos] = 0;
			int crcGiven = strtol(message + pos + 1, 0, 16);
			if (crcGiven != crc) {
				return -1;
			}
		}
		crc = crc16_update(crc, c);
		pos++;
	}
	return argCount;
}


// configure the RGB LED pins and display a startup sequence using the LED
void ledStartUp(byte redPin, byte greenPin, byte bluePin) {
	pinMode(redPin, OUTPUT);
	pinMode(greenPin, OUTPUT);
	pinMode(bluePin, OUTPUT);
	digitalWrite(redPin, HIGH);
	digitalWrite(greenPin, LOW);
	digitalWrite(bluePin, LOW);
	delay(100);
	digitalWrite(redPin, HIGH);
	digitalWrite(greenPin, HIGH);
	digitalWrite(bluePin, LOW);
	delay(100);
	digitalWrite(redPin, LOW);
	digitalWrite(greenPin, HIGH);
	digitalWrite(bluePin, LOW);
	delay(100);
	digitalWrite(redPin, LOW);
	digitalWrite(greenPin, HIGH);
	digitalWrite(bluePin, HIGH);
	delay(100);
	digitalWrite(redPin, LOW);
	digitalWrite(greenPin, LOW);
	digitalWrite(bluePin, HIGH);
	delay(100);
	digitalWrite(redPin, HIGH);
	digitalWrite(greenPin, HIGH);
	digitalWrite(bluePin, HIGH);
	delay(200);
	digitalWrite(redPin, LOW);
	digitalWrite(greenPin, LOW);
	digitalWrite(bluePin, LOW);
}


// produce a random ID number for this board
uint32_t randomId() {
	int seed = 0xf000;
	for (int i = A0; i <= A7; i++) {
		seed += analogRead(i);
	}
	randomSeed(seed);
	return random(1, 0x7fffffff);  // we'll avoid using zero as an ID; random function seems to be signed, so we'll limit it to 31 bits
}


// a marker value used to indicate that the EEPROM has a valid value in it
#define ID_MARKER 0xD42E


// get this device's ID from EEPROM; generate a new one if not already stored in EEPROM
uint32_t getDeviceId() {
	uint16_t marker = 0;
	uint32_t id = 0;
	EEPROM.get(0, marker);
	if (marker == ID_MARKER) {
		EEPROM.get(sizeof(marker), id);
	} else {
		marker = ID_MARKER;
		EEPROM.put(0, marker);
		id = randomId();
		EEPROM.put(sizeof(marker), id);
	}
	return id;
}


// returns true if the checksum on the given serial message is ok (matches the contents of the message);
// if removeChecksum is specified, the message string will be truncated at the (just before) the checksum marker (|)
bool checksumOk(char *message, bool removeChecksum) {
	uint16_t crc = 0xffff;
	int index = 0;
	while (true) {
		char c = message[index];
		if (c == 0) {
			break;
		}
		if (c == '|') {
			if (removeChecksum) {
				message[index] = 0;
			}
			uint16_t crcGiven = strtol(message + index + 1, 0, 16);
			return crcGiven == crc;
		}
		crc = crc16_update(crc, c);
		index++;
	}
	return false;
}
