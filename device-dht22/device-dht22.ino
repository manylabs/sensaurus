#include "SoftwareSerialWithHalfDuplex.h"
#include "CheckStream.h"
#include "Sensaur.h"
#include "DHT.h"


// settings
#define COMM_BAUD 38400
#define COMM_PIN 4
#define MAX_ARGS 5
#define RED_PIN 9
#define GREEN_PIN 10
#define BLUE_PIN 11
#define DHT_PIN 2
#define ARD_LED_PIN 13


// communication line to senaur hub
SoftwareSerialWithHalfDuplex hubSerial(COMM_PIN, COMM_PIN, false, false);
CheckStream hubStream(hubSerial);


// message buffer
#define MESSAGE_BUF_LEN 80
char message[MESSAGE_BUF_LEN];
byte messageIndex = 0;


// semi-unique identifier for this hardware
unsigned long deviceId = 0;


// other globals
DHT dht(DHT_PIN, DHT22);
float temperature = 0;
float humidity = 0;
unsigned long lastSensorReadTime = 0;


void setup() {
  Serial.begin(9600);
  dht.begin();
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
  unsigned long time = millis();
  if (time - lastSensorReadTime > 1000) {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
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
    hubStream.print(temperature);
    hubStream.print(',');
    hubStream.print(humidity);
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
    hubStream.print(";i,temperature,DHT22,degrees C;i,humidity,DHT22,percent");
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

