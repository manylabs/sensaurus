#include "SoftwareSerialWithHalfDuplex.h"
#include "CheckStream.h"
#include "TSL2561.h"
#include "Sensaur.h"


// settings
#define COMM_BAUD 38400
#define COMM_PIN 4
#define MAX_ARGS 5
#define BUTTON_PIN 7
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
TSL2561 tsl(TSL2561_ADDR_FLOAT);
unsigned long lastSensorReadTime = 0;
int lux;  // PPM


void setup() {
  Serial.begin(9600);
  pinMode(COMM_PIN, INPUT);
  hubSerial.begin(COMM_BAUD);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  tsl.begin();
  tsl.setGain(TSL2561_GAIN_0X);
  tsl.setTiming(TSL2561_INTEGRATIONTIME_101MS);
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
    uint32_t lum = tsl.getFullLuminosity();
    uint16_t ir = lum >> 16;
    uint16_t full = lum & 0xFFFF;
    lux = tsl.calculateLux(full, ir);
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
  delay(5);  // wait a moment before responding, since the hub will need a moment to start listening for data

  // query current values
  if (strcmp(cmd, "v") == 0) {
    digitalWrite(ARD_LED_PIN, HIGH);
    hubStream.print("v:");
    hubStream.print(lux);
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
    hubStream.print(";i,light,TSL2561,lux");
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

