#define USE_CHECK_STREAM_BUFFER
#include "SoftwareSerialWithHalfDuplex.h"
#include "CheckStream.h"
#include "Sensaur.h"


// settings
#define COMM_BAUD 38400
#define COMM_PIN 4
#define MAX_ARGS 5
#define RELAY_PIN 2
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
int val1 = 0;
int val2 = 0;


void setup() {
  Serial.begin(9600);
  pinMode(COMM_PIN, INPUT);
  hubSerial.begin(COMM_BAUD);
  deviceId = getDeviceId();
  ledStartUp(RED_PIN, GREEN_PIN, BLUE_PIN);
}


void loop() {

  // handle communication
  while (hubStream.available()) {
    processIncomingByte(hubStream.read());
  }
  while (Serial.available()) {
    processIncomingByte(Serial.read());
  }
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

  // query current values
  if (strcmp(cmd, "v") == 0) {
    digitalWrite(ARD_LED_PIN, HIGH);
    hubStream.print("v:");
    hubStream.print(val1);
    hubStream.print(',');
    hubStream.print(val2);
    hubStream.print(',');
    hubStream.print(val1);
    hubStream.print(',');
    hubStream.print(val2);
    hubStream.println();
    delay(20);
    digitalWrite(ARD_LED_PIN, LOW);

  // set values
  } else if (strcmp(cmd, "s") == 0 && argCount >= 2) {
    val1 = atoi(args[0]);
    val2 = atoi(args[1]);
    digitalWrite(RED_PIN, val1);
    digitalWrite(GREEN_PIN, 0);
    digitalWrite(BLUE_PIN, val2);
    hubStream.print("s:");
    hubStream.print(val1);
    hubStream.print(',');
    hubStream.print(val2);
    hubStream.println();  

  // query metadata
  } else if (strcmp(cmd, "m") == 0) {
    hubStream.print("m:1;");
    hubStream.print(deviceId, HEX);
    hubStream.print(";o,out 1 test");  // note that we need the first 5 chars of type to be unique
    hubStream.print(";o,out 2 test");
    hubStream.print(";i,in 1 test");
    hubStream.print(";i,in 2 test");
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

