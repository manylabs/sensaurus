#ifndef _SENSAUR_H_
#define _SENSAUR_H_
#include "Arduino.h"


// parse a message sent using the sensaur serial protocol
// returns number of args or -1 if error (e.g. checksum error)
// note: parts of the message will get overwritten during parsing
byte parseMessage(char *message, char **command, char *args[], byte maxArgs, char seperator=',');


// configure the RGB LED pins and display a startup sequence using the LED
void ledStartUp(byte redPin, byte greenPin, byte bluePin);


// get this device's ID from EEPROM; generate a new one if not already stored in EEPROM
uint32_t getDeviceId();


// returns true if the checksum on the given serial message is ok (matches the contents of the message);
// if removeChecksum is specified, the message string will be truncated at the (just before) the checksum marker (|)
bool checksumOk(char *message, bool removeChecksum);


#endif  // _SENSAUR_H_
