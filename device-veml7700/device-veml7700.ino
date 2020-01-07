#define USE_CHECK_STREAM_BUFFER
#include "SoftwareSerialWithHalfDuplex.h"
#include "CheckStream.h"
#include "Adafruit_VEML7700.h"
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
Adafruit_VEML7700 veml = Adafruit_VEML7700();
unsigned long deviceId = 0;
int lux = 0;
bool valid = false;


void setup() {
  Serial.begin(9600);
  pinMode(COMM_PIN, INPUT);
  hubSerial.begin(COMM_BAUD);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  if (!veml.begin()) {
    Serial.println("sensor not found");
    while (1);
  }
  veml.setGain(VEML7700_GAIN_1);
  veml.setIntegrationTime(VEML7700_IT_200MS);
  deviceId = getDeviceId();
  ledStartUp(RED_PIN, GREEN_PIN, BLUE_PIN);
  pinMode(ARD_LED_PIN, OUTPUT);
  digitalWrite(ARD_LED_PIN, LOW);
}


void loop() {
  while (hubStream.available()) {
    processIncomingByte(hubStream.read());
  }

  // use this for testing sensors without the hub
  #ifdef TESTING
    readSensor();
    Serial.println(lux);
    delay(1000);
  #endif
}


void processIncomingByte(char c) {
  if (c == 13) {  // respond after second part of newline
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
    if (messageIndex < MESSAGE_BUF_LEN && c != 10) {  // exclude first part of newline
      message[messageIndex] = c;
      messageIndex++;
    }
  }
}


void processMessage(char *cmd, char *args[], int argCount) {
  delay(5);  // wait a moment before responding, since the hub will need a moment to start listening for data

  // query current values
  if (strcmp(cmd, "v") == 0) {
    if (valid == false) {
      readSensor();
    }
    digitalWrite(ARD_LED_PIN, HIGH);
    hubStream.print("v:");
    hubStream.print(lux);
    hubStream.println();
    delay(20);
    digitalWrite(ARD_LED_PIN, LOW);
    readSensor();

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
    hubStream.print(";i,light,VEML7700,lux");
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


void readSensor() {
  lux = veml.readLux();
  valid = true;
}
