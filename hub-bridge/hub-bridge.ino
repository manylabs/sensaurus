#include "SoftwareSerialWithHalfDuplex.h"
#include "CheckStream.h"
#include "Sensaur.h"


// this code is used to control an arduino on a hub board attached to a computer (e.g. raspberry pi); 
// the arduino passes messages between the hub computer and connected devices


#define MAX_DEVICE_COUNT 5
#define HUB_BAUD 38400
#define DEV_BAUD 38400
#define RED_PIN A0
#define GREEN_PIN A1
#define BLUE_PIN A2


// serial connections to each device
SoftwareSerialWithHalfDuplex devSerial[] = {
  SoftwareSerialWithHalfDuplex(2, 2, false, false),
  SoftwareSerialWithHalfDuplex(3, 3, false, false),
  SoftwareSerialWithHalfDuplex(4, 4, false, false),
  SoftwareSerialWithHalfDuplex(5, 5, false, false),
  SoftwareSerialWithHalfDuplex(6, 6, false, false),
};


// serial connections wrapped with objects that add checksums to outgoing messages
CheckStream devStream[] = {
  CheckStream(devSerial[0]),
  CheckStream(devSerial[1]),
  CheckStream(devSerial[2]),
  CheckStream(devSerial[3]),
  CheckStream(devSerial[4]),
};


// buffer for message coming from hub computer
#define HUB_MESSAGE_BUF_LEN 20
char hubMessage[HUB_MESSAGE_BUF_LEN];
byte hubMessageIndex = 0;


// buffer for message coming from sensor/actuator device 
#define DEVICE_MESSAGE_BUF_LEN 80
char deviceMessage[DEVICE_MESSAGE_BUF_LEN];
byte deviceMessageIndex = 0;


// the device we are currently receiving messages from;
// using software serial we can only listen to one device at a time
int listenDevice = -1;


// connection to the hub computer (e.g. raspberry pi)
CheckStream hubStream(Serial);


void setup() {
  Serial.begin(HUB_BAUD);
  for (int i = 0; i < MAX_DEVICE_COUNT; i++) {
    devSerial[i].begin(DEV_BAUD);
  }
  ledStartUp(RED_PIN, GREEN_PIN, BLUE_PIN);
}


void loop() {

  // process any incoming data from the hub computer
  while (Serial.available()) {
    processByteFromComputer(Serial.read());
  }

  // process any incoming data from the device we're currently listening to
  if (listenDevice >= 0) {
    while (devStream[listenDevice].available()) {
      processDeviceMessage(listenDevice);
    }
  }  

  // simple serial bridge; can be used for debugging
  /*
  while (Serial.available()) {
    devStream[0].write(Serial.read());
  }
  while (devStream[0].available()) {
    Serial.write(devStream[0].read());
  }
  */
}


// loop through all the devices, requesting a value from each one
void doPolling() {
  for (int i = 0; i < MAX_DEVICE_COUNT; i++) {
    deviceMessageIndex = 0;
    devSerial[i].listen();
    unsigned long startTime = millis();

    // send request to device
    devStream[i].println("v");

    // wait 100 msec for reply
    do {
      if (processDeviceMessage(i)) {
        break;
      }
    } while (millis() - startTime < 100);  // put this at end so we're less likely to miss first character coming back form device
  }
}


// process an incoming message from a connected device
bool processDeviceMessage(int deviceIndex) {
  while (devStream[deviceIndex].available()) {
    char c = devStream[deviceIndex].read();
    if (c == 10 || c == 13) {
      if (deviceMessageIndex) {  // don't send empty messages
        deviceMessage[deviceMessageIndex] = 0;
        forwardMessageFromDevice(deviceIndex);
      }
      deviceMessageIndex = 0;
      return true;
    } else {
      deviceMessage[deviceMessageIndex] = c;
      if (deviceMessageIndex < DEVICE_MESSAGE_BUF_LEN - 1) {
        deviceMessageIndex++;
      }
    }
  }  
  return false;
}


// pass the last received message from a device to the hub computer
void forwardMessageFromDevice(int deviceIndex) {
  if (checksumOk(deviceMessage, true) == 0) {
    hubStream.print("e:device crc");
    return;
  }
  hubStream.print(deviceIndex);
  hubStream.print('>');
  hubStream.println(deviceMessage);
}


// process any incoming data from the hub computer
void processByteFromComputer(char c) {
  if (c == 10 || c == 13) {
    if (hubMessageIndex) {  // if we have a message from the hub computer
      hubMessage[hubMessageIndex] = 0;
      if (checksumOk(hubMessage, true) == 0) {
        hubStream.println("e:crc");
        hubMessageIndex = 0;
        return;
      }  
      if (hubMessage[0] == 'p') {  // poll all the devices for their current values
        doPolling();
      }
      if (hubMessageIndex > 2 && hubMessage[1] == '>') {  // send a message to a specific device
        int deviceIndex = hubMessage[0] - '0';
        listenDevice = deviceIndex;
        deviceMessageIndex = 0;
        devSerial[deviceIndex].listen();
        devStream[deviceIndex].println(hubMessage + 2);
      }
      hubMessageIndex = 0;
    }
  } else {
    if (hubMessageIndex < HUB_MESSAGE_BUF_LEN - 1) {
      hubMessage[hubMessageIndex] = c;
      hubMessageIndex++;
    }
  }
}


byte deviceIndexToPin(byte deviceIndex) {
  return deviceIndex + 2;
}


byte pinToDeviceIndex(byte pin) {
  return pin - 2;
}

