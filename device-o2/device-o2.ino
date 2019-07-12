#include "SoftwareSerialWithHalfDuplex.h"
#include "CheckStream.h"
#include "Sensaur.h"


// settings
#define COMM_BAUD 38400
#define COMM_PIN 4
#define MAX_ARGS 5
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


// buffer for oxygen data
#define OX_BUF_LEN 80
char g_oxBuf[OX_BUF_LEN];
int g_oxBufIndex = 0;


// latest sensor reading
float g_ox = 0;


// semi-unique identifier for this hardware
unsigned long deviceId = 0;


void setup() {
  Serial.begin(9600);  // note: oxygen sensor is feeding into pin 2 
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

  // read sensor
  checkOx();
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
    hubStream.print(g_ox);
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
    hubStream.print(";i,O2,LOX-02,percent");
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


// check for incoming serial data from oxygen sensor; if whole message is received, parse it
void checkOx() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 10) {
      parseOxBuf();
    } else if (g_oxBufIndex + 1 < OX_BUF_LEN) {
      g_oxBuf[g_oxBufIndex] = c;
      g_oxBufIndex++;
    }
  } 
}


// get index of given character; return -1 if not found
// fix(smaller): replace with strchr
inline int indexOf( const char *str, char c ) {
  int index = 0;
  while (true) {
    char s = *str++;
    if (s == c)
      return index;
    if (s == 0)
      break;
    index++;
  }
  return -1;
}


// parse message from oxygen sensor
// example: "O 0213.7 T +22.2 P 1010 % 021.16 e 0000"
void parseOxBuf() {
  g_oxBuf[g_oxBufIndex] = 0;
  int len = g_oxBufIndex;
  int start = indexOf(g_oxBuf, '%');
  if (start > 0 && start + 2 < len) {
    g_ox = atof(g_oxBuf + start + 2);
  }
  g_oxBufIndex = 0;
}


