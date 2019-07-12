#include "SoftwareSerialWithHalfDuplex.h"
#include "CheckStream.h"
#include "Wire.h"
#include "Sensaur.h"


// settings
#define COMM_BAUD 38400
#define COMM_PIN 4
#define MAX_ARGS 5
#define CO2_ADDR 0x7F
#define RED_PIN 9
#define GREEN_PIN 10
#define BLUE_PIN 11
#define ARD_LED_PIN 13


// communication line to senaur hub
SoftwareSerialWithHalfDuplex hubSerial(COMM_PIN, COMM_PIN, false, false);
CheckStream hubStream(hubSerial);


// message buffer
#define MESSAGE_BUF_LEN 80
char message[MESSAGE_BUF_LEN];
byte messageIndex = 0;


// other globals
unsigned long deviceId = 0;
unsigned long lastSensorReadTime = 0;
int co2;  // PPM


void setup() {
  Wire.begin();
  Serial.begin(9600);
  pinMode(COMM_PIN, INPUT);
  hubSerial.begin(COMM_BAUD);
  deviceId = getDeviceId();
  ledStartUp(RED_PIN, GREEN_PIN, BLUE_PIN);
  pinMode(ARD_LED_PIN, OUTPUT);
  digitalWrite(ARD_LED_PIN, LOW);
}


void loop() {

  // handle communication
  while (hubStream.available()) {
    processIncomingByte(hubStream.read());
  }
  while (Serial.available()) {
    processIncomingByte(Serial.read());
  }

  // read sensor
  unsigned long time = millis();
  if (time - lastSensorReadTime > 1000) {
    int v = readCO2();
    if (v > 0) {
      co2 = v;
    }
    lastSensorReadTime = time;
  }
}


void processIncomingByte(char c) {
  if (c == 10 || c == 13) {
    if (messageIndex) {
      message[messageIndex] = 0;
      char *args[MAX_ARGS];
      char *cmd = NULL;
      int argCount = parseMessage(message, &cmd, args, MAX_ARGS);
      if (argCount >= 0) {
        processMessage(cmd, args, argCount);
      }
      messageIndex = 0;
    }
  } else {
    if (messageIndex < MESSAGE_BUF_LEN) {
      message[messageIndex] = c;
      messageIndex++;
    }
  }
}


void processMessage(char *cmd, char *args[], int argCount) {

  // query current values
  if (strcmp(cmd, "v") == 0) {
    digitalWrite(ARD_LED_PIN, HIGH);
    hubStream.print("v:");
    hubStream.print(co2);
    hubStream.println();
    delay(20);
    digitalWrite(ARD_LED_PIN, LOW);

  // set values
  } else if (strcmp(cmd, "s") == 0 && argCount >= 1) {
    int val = atoi(args[0]);
    hubStream.print("s:");
    hubStream.print(val);
    hubStream.println();  

  // query metadata
  } else if (strcmp(cmd, "m") == 0) {
    hubStream.print("m:1;");
    hubStream.print(deviceId, HEX);
    hubStream.print(";i,CO2,K-30,PPM");
    hubStream.println();

  // respond with error if message not used
  } else {
    hubStream.print("e:");
    hubStream.print(cmd);
    for (int i = 0; i < argCount; i++) {
      if (i) {
        hubStream.print(',');
      } else {
        hubStream.print(';');
      }
      hubStream.print(args[i]);
    }
    hubStream.println();
  }
}


// read currently connected CO2 sensor via I2C; this will return 0 or -1 on read error
// based on code from co2meter.com
int readCO2() {

  // begin write sequence
  Wire.beginTransmission(CO2_ADDR);
  Wire.write(0x22);
  Wire.write(0x00);
  Wire.write(0x08);
  Wire.write(0x2A);
  Wire.endTransmission();
  delay(10);

  // begin read sequence
  Wire.requestFrom(CO2_ADDR, 4);
  byte i = 0;
  byte buffer[4] = {0, 0, 0, 0};
  while (Wire.available()) {
    buffer[i] = Wire.read();
    i++;
  }

  // prep/process value
  int co2Value = 0;
  co2Value |= buffer[1] & 0xFF;
  co2Value = co2Value << 8;
  co2Value |= buffer[2] & 0xFF;

  // check checksum
  byte sum = buffer[0] + buffer[1] + buffer[2];
  if (sum == buffer[3]) {
    return co2Value;
  } else {
    return -1;
  }
}

