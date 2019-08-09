#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include "AWS_IOT.h"
#include "PubSubClient.h"
#include "jled.h"
#include "WiFi.h"
#include "NTPClient.h"
#include "WiFiUdp.h"
#include "ArduinoJson.h"
#include "SimpleHubSerial.h"
#include "CheckStream.h"
#include "Sensaur.h"
#include "SensaurDevice.h"
#include "settings.h"

// set this to INFO OR WARN for production, VERBOSE for debugging
#define RUNTIME_LOG_LOCAL_LEVEL ESP_LOG_INFO
// set to verbose to get more information from components during debugging
// enable esp_log_level_set("*", ESP_LOG_VERBOSE); in setup() and CONFIG_LOG_DEFAULT_LEVEL

// in sdkconfig.h, change CONFIG_LOG_DEFAULT_LEVEL
//  comment out CONFIG_ARDUHAL_ESP_LOG definition to see serial output from ESP_LOGx
// also see 
// https://github.com/espressif/arduino-esp32/issues/565
// /hardware/espressif/esp32/tools/sdk/include/config/sdkconfig.h
// #define CONFIG_LOG_DEFAULT_LEVEL 5
//#define CONFIG_ARDUHAL_ESP_LOG 1

// uncomment this line get more information from sensaurus during debugging
//#define CONFIG_LOG_DEFAULT_LEVEL 5
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

// firmware version for this application: EEPROM will be erased and configuration needed
//  if version is incremented
#define FIRMWARE_VERSION 3
// Use for internal testing when changing version often for different experiments
#define FIRMWARE_VERSION_MINOR 6
#define MAX_DEVICE_COUNT 6
//peters
//#define CONSOLE_BAUD 9600
// peterm
#define CONSOLE_BAUD 9600
#define DEV_BAUD 38400
#define SERIAL_BUFFER_SIZE 120

// Note: for BLE to build, maximum_size specified in boards.txt needs to be adjusted from 1310720 to:
// node32smax.upload.maximum_size=1900544
// and the partition has to be adjusted, e.g. like this:
// # Name,   Type, SubType, Offset,  Size, Flags
// nvs,      data, nvs,     0x9000,  0x5000,
// otadata,  data, ota,     0xe000,  0x2000,
// app0,     app,  ota_0,   0x10000, 0x1D0000,
// app1,     app,  ota_1,   0x1E0000,0x1D0000,
// spiffs,   data, spiffs,  0x3B0000,0x50000,

// peters: preferred default config:
// undef BLE, MQTT, def OTA, AWS_IOT
// uncomment to enable BLE
//#define ENABLE_BLE

// only one of ENABLE_AWS_IOT and ENABLE_MQTT must be defined
// uncomment to enable simple/vanilla mqtt instead of AWS IOT MQTT 
//#define ENABLE_MQTT
// uncomment to allow aws iot connections
#define ENABLE_AWS_IOT
// uncomment to enabled OTA
#define ENABLE_OTA

// used for ESP32 logging
static const char* TAG = "sensaurus";

// max number of time WIFI connection is retried
const int MAX_WIFI_RETRIES = 3;

// BLE mode indicator if 0, we are in wifi/iot-aws mode
// noinit attribute preserves the value during a software reset, thus allowing switching
//   to bleMode without writing to EEPROM
static RTC_NOINIT_ATTR  int bleMode = 0;

// 1 = bleExit
// 2 = bleStart
static int swReboot = 0;
static bool pubsubEnabled = false;
// indicates if settings have been modified and need to be save to EEPROM at "bleExit"
static bool dirty = false;


#define BUTTON_PIN 4
#define STATUS_LED_PIN 5      
#define SERIAL_PIN_1 23
#define SERIAL_PIN_2 25
#define SERIAL_PIN_3 26
#define SERIAL_PIN_4 27
#define SERIAL_PIN_5 32
#define SERIAL_PIN_6 33
#define LED_PIN_1 16
#define LED_PIN_2 17
#define LED_PIN_3 18
#define LED_PIN_4 19
#define LED_PIN_5 21
#define LED_PIN_6 22


#define BLE_SERVICE_UUID "9ec18803-e34a-4882-b61d-864247da821d"
#define WIFI_NETWORK_UUID "e2ccd120-412f-4a99-922b-aca100637e2a"
#define WIFI_PASSWORD_UUID "30db3cd0-8eb1-41ff-b56a-a2a818873c34"
#define OWNER_ID_UUID "af74141f-3c60-425a-9402-62ec79b58c1a"
#define HUB_ID_UUID "e4636699-367b-4838-a421-1904cf95f869"
#define HUB_CERT_UUID "d1c4d088-fd9c-4881-8fc2-656441fa2cf4"
#define HUB_KEY_UUID "f97fee16-f4c3-48ff-a315-38dc2b985770"
#define BLE_CMD_UUID "93311ce4-a1e4-11e9-a3dc-60f81dcdd3b6"
#define MQTT_USER_UUID      "63f04721-b6b4-11e9-99fb-60f81dcdd3b6"
#define MQTT_PASSWORD_UUID  "6617bdbd-b6b4-11e9-b75b-60f81dcdd3b6"
#define MQTT_SERVER_UUID    "6675cf75-b6b4-11e9-82af-60f81dcdd3b6"
#define MQTT_PORT_UUID      "66ebed11-b6b4-11e9-b607-60f81dcdd3b6"

#define BLE_SERVICE_MQTT_UUID "9ec18803-e34b-4882-b61d-864247da821d"

#define BLE_CMD_BLE_EXIT "bleExit"
#define BLE_CMD_BLE_START "bleStart"


// configuration storage (will be in EEPROM)
struct Config {
  int version;
  bool consoleEnabled;
  bool wifiEnabled;
  char wifiNetwork[64];
  char wifiPassword[64];
  int responseTimeout;
  char ownerId[64];
  char hubId[64];
  // if true, use simple MQTT instead of AWS IOT MQTT
  //   but for now, this is given by compilation flag
  //bool simpleMqtt;
  // simple/Vanilla MQTT info
  char mqttServer[128];
  unsigned int mqttPort;
  char mqttUser[64];
  char mqttPassword[64];
  // AWS IOT cert/key
  char thingCrt[1500];
  char thingPrivateKey[2000];
} config;

// set to true to allow ble init at startup. eventually, this should be moved to Config
// as of 7/3/19: ENABLE_AWS_IOT has to be disabled when bleEnable is true.
bool bleEnabled = false;
char commandTopicName[100];  // apparently the topic name string needs to stay in memory
char actuatorsTopicName[100];


unsigned long bootTimeEpoch = 0;
String bootTime;

auto blueLed = JLed(2);

// *******************
// Debug functions ***
// *******************
void displayFreeHeap(const char* title) {
#if LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE
  Serial.print(title);
  Serial.printf("\nHeap size: %d\n", ESP.getHeapSize());
  Serial.printf("Free Heap: %d\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
  Serial.printf("Min Free Heap: %d\n", heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
  Serial.printf("Max Alloc Heap: %d\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#endif  
}

// *******************
// Utility functions * 
// *******************

// Convert incoming BLE array to uint16
uint16_t arrToUint16(uint8_t* arr) {
    // decode the two bytes as little endian
    uint16_t value = (*arr)*256;
    //ESP_LOGD(TAG,  "arrToUint16: valueData[0]=%d", *arr);      
    arr++;
    //ESP_LOGD(TAG,  "arrToUint16: valueData[1]=%d", *arr);      
    value += *arr;
    return value;
}


// *******************
// MQTT functions  ***
// *******************
#ifdef ENABLE_MQTT
const int MAX_MQTT_CONNECT_RETRIES = 3;
static const int MQTT_MAX_BUFFER_SIZE = MQTT_MAX_PACKET_SIZE-10;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
// last time mqtt reconnect was attempted, in millis
static long lastMqttConnectionAttempt = 0;
// interval at which mqtt reconnect can be attempted, in millis
static const int mqttReconnectInterval = 20000;
/**
 * Connect or reconnect to MQTT server and re-subscribe.
 * Returns: true if success, false otherwise.
 */
bool mqttReconnect() {
  // Loop until we're reconnected
  int retries = 0;  
  while (!mqttClient.connected()) {
    if (WiFi.status() != WL_CONNECTED) {
        // see WiFiType.h
        //     WL_DISCONNECTED     = 6
        ESP_LOGE(TAG, "mqttReconnect: Wifi.status() is not WL_CONNECTED (0), but %d. Will not attempt MQTT connection\n", WiFi.status());
        return false;
    }    
    ESP_LOGI(TAG, "Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "hub-";
    //clientId += String(random(0xffff), HEX);
    clientId += String(config.hubId);
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str(),config.mqttUser, config.mqttPassword)) {
      Serial.println("connected");
      // prep topic names
      String topicName = String(config.ownerId) + "/hub/" + config.hubId + "/command";
      strcpy(commandTopicName, topicName.c_str());
      topicName = String(config.ownerId) + "/hub/" + config.hubId + "/actuators";
      strcpy(actuatorsTopicName, topicName.c_str());
      bool rc;
      // do subscriptions
      //rc = mqttClient.subscribe("252/hub/20197/command");
      //if (!rc) {
      //  ESP_LOGE(TAG, "mqttReconnect: mqttClient.subscribe failed for %s: rc=%d, state=%d\n", "252/hub/20197/command", rc, mqttClient.state());
      //}      
      
      rc = mqttClient.subscribe(commandTopicName);
      if (!rc) {
        ESP_LOGE(TAG, "mqttReconnect: mqttClient.subscribe failed for %s: rc=%d, state=%d\n", commandTopicName, rc, mqttClient.state());
        return rc;
      }            
      rc = mqttClient.subscribe(actuatorsTopicName);            
      if (!rc) {
        ESP_LOGE(TAG, "mqttReconnect: mqttClient.subscribe failed for %s: rc=%d, state=%d\n", actuatorsTopicName, rc, mqttClient.state());
      }            
      return rc;
    } else {
      ESP_LOGE(TAG, "mqttReconnect: failed, state=%d. Will try again in 1 second.", mqttClient.state());
      // Wait 1 seconds before retrying
      delay(1000);
      retries++;
    }
    if (retries >= MAX_MQTT_CONNECT_RETRIES) {
      ESP_LOGE(TAG, "mqttReconnect: connect failed: max retries exceeded");
      break;
    }
    
  }
  return false;
}

static volatile bool wifi_connected = false;

void WiFiEvent(WiFiEvent_t event)
{
  ESP_LOGD(TAG, "WiFiEvent: %d", event);
  switch (event)
  {
  case SYSTEM_EVENT_AP_START:
    ESP_LOGD(TAG, "WiFiEvent: %s", "SYSTEM_EVENT_AP_START");
    //can set ap hostname here
    // WiFi.softAPsetHostname(_node_name.c_str());
    //enable ap ipv6 here
    // WiFi.softAPenableIpV6();
    break;

  case SYSTEM_EVENT_STA_START:
    ESP_LOGD(TAG, "WiFiEvent: %s", "SYSTEM_EVENT_STA_START");
    //set sta hostname here
    //WiFi.setHostname(_node_name.c_str());
    break;
  case SYSTEM_EVENT_STA_CONNECTED:
    ESP_LOGD(TAG, "WiFiEvent: %s", "SYSTEM_EVENT_STA_CONNECTED");
    //enable sta ipv6 here
    //WiFi.enableIpV6();
    break;
  case SYSTEM_EVENT_AP_STA_GOT_IP6:
    //both interfaces get the same event
    ESP_LOGD(TAG, "WiFiEvent: %s", "STA IPv6: ");
    Serial.println(WiFi.localIPv6());
    Serial.print("AP IPv6: ");
    Serial.println(WiFi.softAPIPv6());
    break;
  case SYSTEM_EVENT_STA_LOST_IP:
    ESP_LOGD(TAG, "WiFiEvent: %s", "SYSTEM_EVENT_STA_LOST_IP");
    break;    
  case SYSTEM_EVENT_STA_GOT_IP:
    ESP_LOGD(TAG, "WiFiEvent: %s", "SYSTEM_EVENT_STA_GOT_IP");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    wifi_connected = true;
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    wifi_connected = false;
    ESP_LOGD(TAG, "WiFiEvent: SYSTEM_EVENT_STA_DISCONNECTED");
    delay(1000);
    //WiFi.begin(ssid, password);
    break;
  default:
    break;  
  }
}

#endif // ENABLE_MQTT

#ifdef ENABLE_OTA
// setup OTA
void setupOTA() {
  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname(config.hubId);

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      ESP_LOGI(TAG, "Start updating %s", type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  // we need ip address here so that we can test OTA via push from arduino ide
  Serial.print("OTA ready. IP address: ");
  Serial.println(WiFi.localIP());
    
}
#endif // ENABLE_OTA

// dump configuration to console for debugging purposes
void dumpConfig(const Config* c) {
  if (config.consoleEnabled) {    
    //GeneralUtils::dumpInfo();
    uint64_t chipid = ESP.getEfuseMac();
    uint16_t id_high2 = (uint16_t)(chipid>>32);
    uint32_t id_low4 = (uint32_t)chipid;
    Serial.printf("mac=%04X%08X, size=%d; %d,...,%s,%s,%s,%s\n%s, %s, %s, %d\n%s\n%s\n", 
        id_high2, 
        id_low4,
        sizeof(Config), 
        c->version, 
        c->ownerId, 
        c->hubId,
        c->wifiNetwork,
        c->wifiPassword,
        c->mqttUser,
        c->mqttPassword,
        c->mqttServer,
        c->mqttPort,
        c->thingCrt,
        c->thingPrivateKey
        );                
  }
}
#define EEPROM_SIZE sizeof(Config)

// serial connections to each device
SimpleHubSerial devSerial[] = {
  SimpleHubSerial(SERIAL_PIN_1),
  SimpleHubSerial(SERIAL_PIN_2),
  SimpleHubSerial(SERIAL_PIN_3),
  SimpleHubSerial(SERIAL_PIN_4),
  SimpleHubSerial(SERIAL_PIN_5),
  SimpleHubSerial(SERIAL_PIN_6),
};


// serial connections wrapped with objects that add checksums to outgoing messages
CheckStream devStream[] = {
  CheckStream(devSerial[0]),
  CheckStream(devSerial[1]),
  CheckStream(devSerial[2]),
  CheckStream(devSerial[3]),
  CheckStream(devSerial[4]),
  CheckStream(devSerial[5]),
};


// buffer for message coming from USB serial port
#define CONSOLE_MESSAGE_BUF_LEN 40
char consoleMessage[CONSOLE_MESSAGE_BUF_LEN];
byte consoleMessageIndex = 0;


// buffer for message coming from sensor/actuator device 
#define DEVICE_MESSAGE_BUF_LEN 200
char deviceMessage[DEVICE_MESSAGE_BUF_LEN];
byte deviceMessageIndex = 0;


// other globals
bool configMode = false;
unsigned long sendInterval = 0;
unsigned long lastSendTime = 0;
unsigned long pollInterval = 1000;
unsigned long lastPollTime = 0;
Device devices[MAX_DEVICE_COUNT];
int ledPin[MAX_DEVICE_COUNT] = {LED_PIN_1, LED_PIN_2, LED_PIN_3, LED_PIN_4, LED_PIN_5, LED_PIN_6};
AWS_IOT awsConn;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
unsigned long lastEpochSeconds = 0;  // seconds since epoch start (from NTP)
unsigned long lastTimeUpdate = 0;  // msec since boot
unsigned long lastMemoryDisplay = 0;


// run once on startup
void setup() {

  // uncomment this line to get more information from components during debugging
  // Use ESP_LOG_WARN, ESP_LOG_INFO, ESP_LOG_DEBUG, ESP_LOG_VERBOSE to get less or more verbosity
  // A good value for production release is ESP_LOG_WARN or ESP_LOG_INFO
  // peters: set to WARN/INFO for production
  esp_log_level_set("*", ESP_LOG_WARN);
  esp_log_level_set(TAG, ESP_LOG_INFO);
  
  // init. bleMode persistent variable if needed 
  // ESP_RST_SW Software reset via esp_restart.
  esp_reset_reason_t reason = esp_reset_reason();
  //if ((reason != ESP_RST_DEEPSLEEP) && (reason != ESP_RST_SW)) {
  // initialize if this is not a software reset
  if (reason != ESP_RST_SW) {
    bleMode = 0;
  }

  // prepare serial connections
  Serial.begin(CONSOLE_BAUD);
  Serial.println();
  initConfig();
  Serial.println("starting");
  for (int i = 0; i < MAX_DEVICE_COUNT; i++) {
    devSerial[i].begin(DEV_BAUD);
  }

  // prepare LED pins and button
  for (int i = 0; i < MAX_DEVICE_COUNT; i++) {
    pinMode(ledPin[i], OUTPUT);
    digitalWrite(ledPin[i], LOW);
  }
  ledcAttachPin(STATUS_LED_PIN, 0);  // attach status LED to PWM channel 0
  ledcSetup(0, 5000, 8);  // set up channel 0 to use 5000 Hz with 8 bit resolution
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // connect to wifi  
  int status = WL_IDLE_STATUS;
  if (config.wifiEnabled) {
    int retries = 0;
    while (status != WL_CONNECTED) {
      // enable onEvent call if need to trace detailed wifi behavior
      // peters: disabled in production
      //WiFi.onEvent(WiFiEvent);      
      status = WiFi.begin(config.wifiNetwork, config.wifiPassword);
      if (status != WL_CONNECTED) {
        delay(2000);
        if (retries >= MAX_WIFI_RETRIES) {
          Serial.printf("failed connecting to wifi %s\n", config.wifiNetwork);
          break;
        }
        retries++;
      } else {
          Serial.println("connected to wifi");   
          Serial.println("IP address: ");
          Serial.println(WiFi.localIP());
          while (WiFi.status() != WL_CONNECTED) {
            delay(100);
            Serial.print(".");
          }
          
          Serial.println("after WiFi.status() wait: IP address: ");
          Serial.println(WiFi.localIP());
          
      }
    }
  } else {
    Serial.println("wifiEnabled is false: not connected to wifi.");    
  }
  
  setStatusLED(HIGH);

  // see if config button is pressed
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("config button pressed: switching to ble mode");
    bleMode = true;
  }
  if (status != WL_CONNECTED) {
    Serial.println("wifi not connected - forcing ble mode");
    bleMode = true;
  }
  Serial.printf("bleMode=%d\n", bleMode);
  bool awsIotConnected = false;
#ifdef ENABLE_AWS_IOT 
  // connect to AWS MQTT
  // note: some AWS IoT code based on https://github.com/jandelgado/esp32-aws-iot
  if (!bleMode && status == WL_CONNECTED ) {
    if (awsConn.connect(HOST_ADDRESS, CLIENT_ID, aws_root_ca_pem, config.thingCrt, config.thingPrivateKey) == 0) {
      Serial.println("connected to AWS");
      awsIotConnected = true;
      delay(200);  // wait a moment before subscribing

      // prep topic names
      String topicName = String(config.ownerId) + "/hub/" + config.hubId + "/command";
      strcpy(commandTopicName, topicName.c_str());
      topicName = String(config.ownerId) + "/hub/" + config.hubId + "/actuators";
      strcpy(actuatorsTopicName, topicName.c_str());

      // do subscriptions
      subscribe(commandTopicName);
      subscribe(actuatorsTopicName);
      pubsubEnabled = true;
    } else {
      Serial.println("failed to connect to AWS");
      //freezeWithError();
    }
  } else {
      Serial.println("Skipped connecting to AWS IOT");    
  }
  
#elif defined(ENABLE_MQTT)
  mqttClient.setServer(config.mqttServer, config.mqttPort);
  mqttClient.setCallback(mqttMessageHandler);
  
  bool rc = mqttReconnect();
  
  pubsubEnabled = true;     
#endif // ENABLE_AWS_IOT 
  
  
  if (!awsIotConnected) {
  // if AWS IOT is not connected, we have enough memory to run BLE
  // we could use pubsubEnabled instead of awsIotConnected if we wanted to disabled  BLE
  //  when both aws iot and simple mqtt are up.
  //if (!pubsubEnabled) {
    Serial.println("Enabling BLE.");
    bleMode++;
  }
#ifdef ENABLE_BLE
  if (bleMode) {
    startBLE();      
  }
#endif // ENABLE_BLE

  if (status == WL_CONNECTED) {

    // get network time
    timeClient.begin();
    timeClient.setTimeOffset(0);  // we want UTC
    updateTime();
    bootTime = timeClient.getFormattedTime(); 
    bootTimeEpoch = timeClient.getEpochTime();

#ifdef ENABLE_OTA
    // peterm: delay needed, otherwise OTA won't work
    if (status == WL_CONNECTED) {
      delay(100);
      setupOTA();
    }
#endif
  }

  if (status == WL_CONNECTED) {
    // send current status
    sendStatus();
  }
  Serial.println("ready");
}


// run repeatedly
void loop() {
#ifdef ENABLE_OTA
  ArduinoOTA.handle();
#endif
#ifdef ENABLE_MQTT
   mqttClient.loop();
  if (pubsubEnabled) {
    if (!mqttClient.connected() || mqttClient.state() != MQTT_CONNECTED) {
      unsigned long time = millis();
      if (time - lastMqttConnectionAttempt > mqttReconnectInterval) {
        // see PubSubClient.h
        //#define MQTT_CONNECTION_TIMEOUT     -4
        //#define MQTT_CONNECTION_LOST        -3
        //#define MQTT_CONNECT_FAILED         -2
        //#define MQTT_DISCONNECTED           -1
        //#define MQTT_CONNECTED               0

        ESP_LOGI(TAG, "MQTT client disconnected state=%d, WiFi.status()=%d: trying reconnect...", 
          mqttClient.state(), WiFi.status());
         
        bool delayReconnect = false;
        if (WiFi.status() == WL_CONNECTED) {
          // try dns. If that fails we consider wifi to be down, even if it shows wifi.status() OK (this is a bug in core code)
          //remote host to test with DNS to find out if wifi is really up.
          // not that this may not work in conditions where system is working without outside
          //  of internet, e.g. if it has its own mqtt server and no inernet connectivity
          //  using google.com, but could also use aws server if using aws iot.
          const char* remoteHostTest = "www.google.com";
          IPAddress remote_addr;
      
          if (WiFi.hostByName(remoteHostTest, remote_addr)) {
            ESP_LOGD(TAG, "WiFi.hostByName() succeeded. Wifi is up.");
          } else {
            ESP_LOGW(TAG, "WiFi.hostByName() failed. Wifi may be down: restarting Wifi...");
            delayReconnect = true;
            WiFi.begin(config.wifiNetwork, config.wifiPassword);                                    
          }
          /*
          IPAddress ip = WiFi.localIP();
          bool rc = Ping.ping(ip);
          String ipStr = String(ip.toString());            
          if (!rc) {
            ESP_LOGW(TAG, "WiFi.status() is WL_CONNECTED) but ping to my ip %s failed. Restarting Wifi...", 
              ipStr.c_str()); 
            WiFi.begin(config.wifiNetwork, config.wifiPassword);            
          } else {
            ESP_LOGD(TAG, "ping to my ip %s succeeded.", 
              ipStr.c_str());
               
          }
          */
        } else {
          // TODO: add handling of a normal wifi disconnect
          ESP_LOGE(TAG, "mqttReconnect: Wifi.status() is not WL_CONNECTED (0), but %d. Add code in loop to restart WiFi.begin()\n", WiFi.status());          
        }
        if (!delayReconnect) {
          bool rc = mqttReconnect();
          if (!rc) {
            ESP_LOGI(TAG, "MQTT reconnect attempt failed. Will try again in %d seconds", mqttReconnectInterval/1000);
          }          
          lastMqttConnectionAttempt = millis();
        } else {
          // TODO: allow 10 more seconds to wifi fully connect before retrying.
          //lastMqttConnectionAttempt += 10000;
        }
      }      
    }
  }  
#endif  
  // process any incoming data from the hub computer
  while (Serial.available()) {
    processByteFromComputer(Serial.read());
  }

  // yield to other tasks to allow AWS messages to be received
  taskYIELD();
  
  if (swReboot) {
    Serial.println("Rebooting on request...");
    // wait for BLE processing to settle
    delay(500);
    esp_restart();    
  }

  // do polling
  if (pollInterval) {
    unsigned long time = millis();
    if (time - lastPollTime > pollInterval) {
      doPolling();
      if (lastPollTime) {
        lastPollTime += pollInterval;  // increment poll time so we don't drift
      } else {
        lastPollTime = time;  // unless this is the first time polling
      }
    }

    // check for disconnects
    time = millis();
    for (int i = 0; i < MAX_DEVICE_COUNT; i++) {
      Device &d = devices[i];
      if (d.connected() && d.noResponseCount() >= 2) {
        d.setConnected(false);
        d.resetComponents();  // we use component count to decide whether to request; might as well request metadata rather than values on disconnected devices
        digitalWrite(ledPin[i], LOW);
        sendDeviceInfo();   
      }
    }
  }

  // send current sensor values to server
  unsigned long time = millis();
  if (sendInterval) {
    if (time - lastSendTime > sendInterval) {
      sendSensorValues(time);
      if (lastSendTime) {
        lastSendTime += sendInterval;  // increment send time so we don't drift
      } else {
        lastSendTime = time;  // unless this is the first time sending
      }
    }
  }
  
  // check for BLE config button (button will be LOW when pressed)
  if ((digitalRead(BUTTON_PIN) == LOW) && configMode == false) {
    Serial.println("Button pressed.");
    configMode = true;
    bleMode++;
    Serial.printf("bleMode=%d\n", bleMode);
    delay(100);
    esp_restart();
  }
  if (configMode) {
    ledcWrite(0, (millis() >> 3) & 255);  // fade LED when in config mode
  }

  // display memory usage
  if (config.consoleEnabled) {
    if (time - lastMemoryDisplay > 5 * 60 * 1000) {
      displayFreeHeap("loop");
      lastMemoryDisplay = time;
    }
  }

  // get network time once an hour
  if (time - lastTimeUpdate > 1000 * 60 * 60) {
    updateTime();
  }
  blueLed.Update();
}


// loop through all the devices, requesting a value from each one
void doPolling() {
  for (int i = 0; i < MAX_DEVICE_COUNT; i++) {
    if (devices[i].componentCount()) {
      devStream[i].println('v');  // request values (if any)
    } else {
      devStream[i].println('m');  // request metadata if not yet received
    }
    waitForResponse(i);
  }
}


void subscribe(const char *topicName) {
  if (awsConn.subscribe(topicName, awsMessageHandler) == 0) {
    Serial.print("subscribed to topic: ");
    Serial.println(topicName);
  } else {
    Serial.print("failed to subscribe to topic: ");
    Serial.println(topicName);
    freezeWithError();
  }
}


// handle an incoming command MQTT message
void awsMessageHandler(char *topicName, int payloadLen, char *payLoad) {
  handleIncomingMessage(topicName, payloadLen, payLoad);
}

void mqttMessageHandler(char* topic, byte *payload, unsigned int length) {
  //Serial.println("-------new message from broker-----");
  handleIncomingMessage(topic, length, (char*)payload);  
}

void handleIncomingMessage(char *topicName, int payloadLen, char *payLoad) {
  DynamicJsonDocument doc(256);
  deserializeJson(doc, payLoad);
  if (doc.containsKey("command")) {
    String command = doc["command"];
    runCommand(command.c_str(), doc);
  } else {
    updateActuators(doc);
  }
}


void waitForResponse(int deviceIndex) {
  SimpleHubSerial &ser = devSerial[deviceIndex];
  ser.startRead();
  deviceMessageIndex = 0;

  // read a message into our buffer
  unsigned long startTime = millis();
  do {
    char c = (char) ser.readByte(config.responseTimeout);
    if (c < 32) {
      break;
    } else {
      if (deviceMessageIndex < DEVICE_MESSAGE_BUF_LEN - 1) {
        deviceMessage[deviceMessageIndex++] = c;
      }
    }
  } while (millis() - startTime < config.responseTimeout);  // put this at end so we're less likely to miss first character coming back form device
  ser.endRead();

  // process the message
  if (deviceMessageIndex) {
    Device &d = devices[deviceIndex];
    if (d.connected() == false) {
      d.setConnected(true);
      d.responded();  // reset the no-response counter
      d.resetComponents();  // clear out all the components until we get a meta-data message
    }
    deviceMessage[deviceMessageIndex] = 0;
    processMessageFromDevice(deviceIndex);
    devices[deviceIndex].responded();
  } else {
    devices[deviceIndex].noResponse();
  }
}


void processMessageFromDevice(int deviceIndex) {

  // if enabled, echo the message to the USB serial console
  if (config.consoleEnabled) {
    Serial.print(deviceIndex + 1);  // display plug number not device index
    Serial.print('>');
    Serial.println(deviceMessage);
  }

  if (checksumOk(deviceMessage, true) == 0) {
    if (config.consoleEnabled) {
      Serial.printf("e:checksum error on plug %d\n", deviceIndex + 1);
    }
    return;
  }

  // at this point we'll assume it's a valid message and update the last message time, which we use to detect disconnects
  Device &dev = devices[deviceIndex];
  
  // process the message
  char *command;
  char *args[MAX_COMPONENT_COUNT + 2];  // the meta-data message has version, ID, and string per component
  if (deviceMessage[0] == 'v') {  // values
    int argCount = parseMessage(deviceMessage, &command, args, MAX_COMPONENT_COUNT + 1);
    int argIndex = 0;
    for (int i = 0; i < dev.componentCount(); i++) {
      Component &c = dev.component(i);
      if (c.dir() == 'i' && argIndex < argCount) {
        c.setValue(args[argIndex]);
        argIndex++;
      }
    }
    
  } else if (deviceMessage[0] == 'm') {  // metadata
    int argCount = parseMessage(deviceMessage, &command, args, MAX_COMPONENT_COUNT + 1, ';');  // note: using semicolon as separator here
    if (argCount > 2) {
      dev.setVersion(args[0]);
      dev.setId(args[1]);

      // populate component info
      int componentCount = argCount - 2;
      if (componentCount > MAX_COMPONENT_COUNT) {
        componentCount = MAX_COMPONENT_COUNT;
      }
      dev.setComponentCount(componentCount);
      for (int i = 0; i < componentCount; i++) {
        dev.component(i).setInfo(args[i + 2]);
      }

      // once we have metadata, we can indicate that the device has successfully connected
      digitalWrite(ledPin[deviceIndex], HIGH);

      // send device/component info to server
      sendDeviceInfo();
    }
  }
}


// process any incoming data from the hub computer
void processByteFromComputer(char c) {
  if (c == 10 || c == 13) {
    if (consoleMessageIndex) {  // if we have a message from the hub computer
      consoleMessage[consoleMessageIndex] = 0;
      DynamicJsonDocument doc(10);
      runCommand(consoleMessage, doc);
      consoleMessageIndex = 0;
    }
  } else {
    if (consoleMessageIndex < CONSOLE_MESSAGE_BUF_LEN - 1) {
      consoleMessage[consoleMessageIndex] = c;
      consoleMessageIndex++;
    }
  }
}


void runCommand(const char *command, DynamicJsonDocument &doc) {
  Serial.printf("runCommand: command=%s\n", command);
  if (strcmp(command, "p") == 0) {  // poll all the devices for their current values
    Serial.println("polling");
    doPolling();
  } else if (strcmp(command, "d") == 0) {  // dump settings
    Serial.println("settings currently effective:");    
    dumpConfig(&config);        
  } else if (strcmp(command, "w") == 0) {  // eepromwrite test
    EEPROM.put(0, config);
    EEPROM.commit();     
    Serial.println("settings have been saved to EEPROM:");      
    dumpConfig(&config);        
  } else if (strcmp(command, "r") == 0) {  // eepromread test
    Config myConfig;
    EEPROM.get(0, myConfig);
    Serial.println("settings currently in EEPROM:");
    dumpConfig(&myConfig);  

  } else if (strcmp(command, "wifi_begin") == 0) {  // restart wifi
      int status = WL_IDLE_STATUS;
      status = WiFi.begin(config.wifiNetwork, config.wifiPassword);
      if (status == WL_CONNECTED) {
          ESP_LOGD(TAG, "connected to wifi, IP address: ");   
          Serial.println(WiFi.localIP());
      } else {
        ESP_LOGE(TAG, "reconnect to wifi failed: staus=%d", status);
      }
  } else if (strcmp(command, "s") == 0) {  // start sending sensor values once a second
    pollInterval = 1000;
    sendInterval = 1000;
  } else if (strcmp(command, "start_ble") == 0) {  // invoke startBLE without rebooting
    #ifdef ENABLE_BLE
      startBLE();
    #endif
  } else if (strcmp(command, "req_status") == 0) {  // request a status message
    sendStatus();
  } else if (strcmp(command, "req_devices") == 0) {  // request a devices message
    sendDeviceInfo();
  } else if (strcmp(command, "set_send_interval") == 0) {
    sendInterval = round(1000.0 * doc["send_interval"].as<float>());
    if (sendInterval < 1000) {
      pollInterval = 500;
    } else {
      pollInterval = 1000;
    }
  } else if (strcmp(command, "update_firmware") == 0) {
    String url = doc["url"];
    Serial.print("updating firmware: ");
    Serial.println(url);
  } else if (command[0] && command[1] == '>') {  // send a message to a specific device
    int deviceIndex = command[0] - '0';
    if (config.consoleEnabled) {
      Serial.print("sending message to device ");
      Serial.print(deviceIndex);
      Serial.print(": ");
      Serial.println(command + 2);
    }
    devStream[deviceIndex].println(command + 2);
    waitForResponse(deviceIndex);
  } else if (!strcmp(command, BLE_CMD_BLE_START)) { // startBLE: reboot into BLE mode
      // mark unit for software reboot (start BLE mode)
      bleMode++;
      swReboot = 2;
  } else if (!strcmp(command, BLE_CMD_BLE_EXIT)) { // exitBLE: reboot into AWS IOT mode
      // mark unit for software reboot (exit BLE mode)
      bleMode = 0;
      swReboot = 1;
  } else {
      ESP_LOGD(TAG, "command not recognized: %s", command);       
  }
}


void updateActuators(DynamicJsonDocument &doc) {
  JsonObject obj = doc.as<JsonObject>();

  // loop over devices; we'll send a message to each one (that has updated actuators)
  for (int deviceIndex = 0; deviceIndex < MAX_DEVICE_COUNT; deviceIndex++) {
    Device &d = devices[deviceIndex];
    bool deviceUpdated = false;  // will be made true if the incoming message specified a new value for any component of this device

    // look for any new actuator values for this device
    for (JsonPair p : obj) {
      String componentId(p.key().c_str());
      int dashPos = componentId.indexOf('-');
      String deviceId = componentId.substring(0, dashPos);
      if (deviceId == d.id()) {  // probably faster to use C string comparison
        String idSuffix = componentId.substring(dashPos + 1);
        for (int i = 0; i < d.componentCount(); i++) {
          Component &c = d.component(i);
          if (idSuffix == c.idSuffix()) {  // probably faster to use C string comparison rather than create a String object on the fly
            c.setActuatorValue(p.value());
            deviceUpdated = true;
          }
        }
      }
    }

    // if we updated this device, send a message to it
    // we construct the message containing a value for each output component, in the order specified via the device metadata
    if (deviceUpdated) {
      Stream &s = devStream[deviceIndex];
      s.print("s:");
      bool first = true;
      for (int i = 0; i < d.componentCount(); i++) {
        Component &c = d.component(i);
        if (c.dir() == 'o') {
          if (first == false) {
            s.print(',');
          }
          s.print(c.actuatorValue());
          first = false;
        }
      }
      s.println();
      waitForResponse(deviceIndex);
    }
  }
}


void sendStatus() {
  if (config.consoleEnabled) {
    Serial.print("sending status: ");
  }
  DynamicJsonDocument doc(256);
  doc["wifi_network"] = config.wifiNetwork;
  doc["version"] = config.version;
  // peters: disable for production
  // doc["minorVersion"] = FIRMWARE_VERSION_MINOR;
  doc["host"] = HOST_ADDRESS;
  // useful for testing OTA, wifi bug tracking etc.
  //   it can be removed later
  // ---- start of debug info
  //
  String localIp = String(WiFi.localIP().toString());
  doc["localIP"] = localIp;
  doc["built"] = __TIMESTAMP__;  // useful for tracing updated firmware where version has not changed, such as when testing OTA
  doc["uptime"] = millis();
  String now = timeClient.getFormattedTime(); 
  //Time.format("%Y-%m-%d %H:%M:%S");
  doc["bootTime"] = bootTime;
  doc["bootTimeEpoch"] = bootTimeEpoch;
  doc["hubTime"] = now;
  // ---- end of debug info
  String topicName = String(config.ownerId) + "/hub/" + config.hubId + "/status";
  String message;
  serializeJson(doc, message);
  if (pubsubEnabled) {
    if (hubPublish(topicName.c_str(), message.c_str())) {
      Serial.println("error publishing");
    }
  }  
  if (config.consoleEnabled) {
    Serial.println("done");
  }
}

// return 0 if success, >= 1 if failure
int hubPublish(const char* topic, const char* message) {
  int ret;
#ifdef ENABLE_AWS_IOT 
  ESP_LOGD(TAG, "awsConn.publish: %s: %s", topic, message);
  ret = awsConn.publish(topic, message);
#elif defined(ENABLE_MQTT)
  ESP_LOGD(TAG, "mqttClient.publish: %s: %s", topic, message);
  if (!mqttClient.connected()) {
    ESP_LOGE(TAG, "mqttClient.publish: error: not connected");    
    return 1;
  }
  bool rc = mqttClient.beginPublish(topic, strlen(message), false);  
  if (!rc)  {
    ESP_LOGE(TAG, "mqttClient.beginPublish failed: rc=%d, state=%d\n", rc, mqttClient.state());
  } else {
    int remainingLength = strlen(message);
    while (remainingLength > 0) {
      int toSend = min(remainingLength, MQTT_MAX_BUFFER_SIZE); 
      int offset = strlen(message)-remainingLength;
      rc = mqttClient.write((const byte*)&message[offset], toSend);
      if (!rc) {
        ESP_LOGE(TAG, "mqttClient.write failed: rc=%d, state=%d\n", rc, mqttClient.state());    
        // use goto because it simplifies logic and makes processing
        //  less error prone
        goto do_return;
      }
      //ESP_LOGD(TAG, "mqttClient.write: wrote %d bytes from offset %d\n", toSend, offset);    
      remainingLength -= toSend;      
    } 
    rc = mqttClient.endPublish();
    if (!rc) {
      ESP_LOGE(TAG, "mqttClient.endPublish failed: rc=%d, state=%d\n", rc, mqttClient.state());    
    }      
  }    
  do_return:
  if (!rc) {
    ESP_LOGE(TAG, "mqttClient publishing failed: rc=%d, state=%d\n", rc, mqttClient.state());
  }
  ret = rc ? 0:1;
#endif ENABLE_AWS_IOT    
  if (!ret) {
    // blink if publish success
    blueLed.Blink(10, 10);
  }
  return ret;
}

void sendDeviceInfo() {
  String json = "{";
  bool first = true;
  int deviceCount = 0;
  for (int i = 0; i < MAX_DEVICE_COUNT; i++) {
    Device &dev = devices[i];
    if (dev.connected()) {
      if (first == false) {
        json += ",";
      }
      json += '"';
      json += dev.id();
      json += "\":";
      json += String("{\"version\":") + dev.version() + ", \"plug\":" + (i + 1) + ", \"components\": [";
      for (int j = 0; j < dev.componentCount(); j++) {
        if (j)
          json += ',';
        json += dev.component(j).infoJson();
      }
      json += "]}";
      first = false;
      String topicName = String(config.ownerId) + "/device/" + dev.id();
      if (config.wifiEnabled) {
        String message = String("{\"hub_id\":\"") + config.hubId + "\"}";
        if (hubPublish(topicName.c_str(), message.c_str())) {  // send hub ID for this device
          Serial.println("error publishing");
        }
      }
      deviceCount++;
    }
  }
  json += "}";
  String topicName = String(config.ownerId) + "/hub/" + config.hubId + "/devices";
  if (config.wifiEnabled) {
    if (config.consoleEnabled) {
      Serial.printf("sending device info; size: %d\n", json.length());
    }
    if (hubPublish(topicName.c_str(), json.c_str())) {  // send list of device info dictionaries
      Serial.printf("error publishing; message size: %d\n", json.length());
    }
  }
  if (config.consoleEnabled) {
    Serial.print(deviceCount);
    Serial.println(" devices");
  }
}


void sendSensorValues(unsigned long time) {
  if (config.consoleEnabled) {
    Serial.print("sending values: ");
  }
  int valueCount = 0;
  DynamicJsonDocument doc(512);
  String wallTime = String(((double) (time - lastTimeUpdate) / 1000.0) + (double) lastEpochSeconds);  // convert to string since json code doesn't seem to handle doubles correctly
  doc["time"] = wallTime;
  for (int i = 0; i < MAX_DEVICE_COUNT; i++) {
    Device &d = devices[i];
    if (d.connected()) {
      for (int j = 0; j < d.componentCount(); j++) {
        Component &c = d.component(j);
        if (c.dir() == 'i') {
          String compId = String(d.id()) + '-' + c.idSuffix();
          doc[compId] = c.value();
          valueCount++;
        }
      }
    }
  }
  String topicName = String(config.ownerId) + "/hub/" + config.hubId + "/sensors";
  String message;
  serializeJson(doc, message);
  if (config.wifiEnabled) {
    if (hubPublish(topicName.c_str(), message.c_str())) {
      Serial.println("error publishing");
    }
  }
  if (config.consoleEnabled) {
    Serial.print(valueCount);
    Serial.print(" values at ");
    Serial.println(wallTime);
  }
}


void testLEDs() {
  digitalWrite(STATUS_LED_PIN, HIGH);
  delay(500);
  digitalWrite(STATUS_LED_PIN, LOW);
  for (int i = 0; i < MAX_DEVICE_COUNT; i++) {
    digitalWrite(ledPin[i], HIGH);
    delay(200);
    digitalWrite(ledPin[i], LOW);
  }
}


void initConfig() {
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("Failed to initialise EEPROM");
    Serial.println("Restarting...");
    delay(1000);
    ESP.restart();
  }  

  // load configuration from EEPROM if available
  EEPROM.get(0, config);
  Serial.println("config loaded from flash memory:");
  if (RUNTIME_LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE) {
    dumpConfig(&config);    
  }

  if (config.version != FIRMWARE_VERSION || config.version == 0) {
    if (config.version) {
      Serial.printf("Firmware version changed from %d to %d. Perform re-configuration of this hub!\n", config.version, FIRMWARE_VERSION);
    } else {
      Serial.println("Firmware version loaded from EEPROM is 0. Perform configuration of this hub!");
    }
    config.version = FIRMWARE_VERSION;
    config.consoleEnabled = ENABLE_CONSOLE;
    config.wifiEnabled = ENABLE_WIFI;
    config.responseTimeout = RESPONSE_TIMEOUT;
    strncpy(config.ownerId, OWNER_ID, 64);
    strncpy(config.hubId, HUB_ID, 64);
    strncpy(config.wifiNetwork, WIFI_SSID, sizeof(config.wifiNetwork)-1);
    strncpy(config.wifiPassword, WIFI_PASSWORD, sizeof(config.wifiPassword)-1);
    //config.simpleMqtt = SIMPLE_MQTT;    
    strncpy(config.mqttUser, MQTT_USER, sizeof(config.mqttUser)-1);
    strncpy(config.mqttPassword, MQTT_PASSWORD, sizeof(config.mqttPassword)-1);
    strncpy(config.mqttServer, MQTT_SERVER, sizeof(config.mqttServer)-1);
    config.mqttPort = MQTT_PORT;    
    strncpy(config.thingCrt, certificate_pem_crt, sizeof(config.thingCrt)-1);
    strncpy(config.thingPrivateKey, private_pem_key, sizeof(config.thingPrivateKey)-1);
  } else {
    // loaded config seems OK.  
    Serial.printf("Loaded config - firmware version=%d.\n", config.version);
  }
}


void setStatusLED(int state) {
  if (state) {
    ledcWrite(0, 255);
  } else {
    ledcWrite(0, 0);
  }
}


void updateTime() {

  // get new time from network
  int counter = 0;
  while (!timeClient.update()) {
    timeClient.forceUpdate();
    delay(1000);
    counter++;
    if (counter > 10) {
      break;  // failed; try again later
    }
  }

  lastTimeUpdate = millis();
  lastEpochSeconds = timeClient.getEpochTime();
  if (config.consoleEnabled) {
    Serial.print("updated time: ");
    Serial.println(lastEpochSeconds);
  }
}


void freezeWithError() {
  while (true) {
    setStatusLED(HIGH);
    delay(1000);
    setStatusLED(LOW);
    delay(1000);
  }
}


#ifdef ENABLE_BLE


class WifiNetworkCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) {
    std::string value = characteristic->getValue();
    if (strcmp(config.wifiNetwork, value.c_str())) {
      dirty = true;
      strncpy(config.wifiNetwork, value.c_str(), sizeof(config.wifiNetwork));
    }    
    //Serial.println(config.wifiNetwork);
    ESP_LOGI(TAG, "%s", config.wifiNetwork);          
  }
};


class WifiPasswordCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) {
    std::string value = characteristic->getValue();
    if (strcmp(config.wifiPassword, value.c_str())) {
      dirty = true;
      strncpy(config.wifiPassword, value.c_str(), sizeof(config.wifiPassword));
    }
    //Serial.println(config.wifiPassword);
    ESP_LOGI(TAG,  "%s", config.wifiPassword);      
  }
};


class OwnerIdCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) {
    std::string value = characteristic->getValue();
    if (strcmp(config.ownerId, value.c_str())) {
      dirty = true;
      strncpy(config.ownerId, value.c_str(), sizeof(config.ownerId));
    }
    //Serial.println(config.ownerId);
    ESP_LOGI(TAG,  "%s", config.ownerId);
  }
};


class HubIdCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) {
    std::string value = characteristic->getValue();
    if (strcmp(config.hubId, value.c_str())) {
      dirty = true;
      strncpy(config.hubId, value.c_str(), sizeof(config.hubId));
    }
    //Serial.println(config.hubId);
    ESP_LOGI(TAG,  "%s", config.hubId);      
  }
};

class MqttUserCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) {
    std::string value = characteristic->getValue();
    if (strcmp(config.mqttUser, value.c_str())) {
      dirty = true;
      strncpy(config.mqttUser, value.c_str(), sizeof(config.mqttUser));
    }
    ESP_LOGI(TAG,  "%s", config.mqttUser);      
  }
};

class MqttPasswordCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) {
    std::string value = characteristic->getValue();
    if (strcmp(config.mqttPassword, value.c_str())) {
      dirty = true;
      strncpy(config.mqttPassword, value.c_str(), sizeof(config.mqttPassword));
    }
    ESP_LOGI(TAG,  "%s", config.mqttPassword);      
  }
};

class MqttServerCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) {
    std::string value = characteristic->getValue();
    if (strcmp(config.mqttServer, value.c_str())) {
      dirty = true;
      ESP_LOGD(TAG,  "MqttServerCallbacks modifed %s->%s", config.mqttServer, value.c_str());      
      strncpy(config.mqttServer, value.c_str(), sizeof(config.mqttServer));
    }
    ESP_LOGI(TAG,  "MqttServerCallbacks: %s", config.mqttServer);      
  }
};

class MqttPortCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) {
    uint16_t value = arrToUint16(characteristic->getData());
    if (config.mqttPort != value) {
      dirty = true;
      config.mqttPort = value;
    }
    ESP_LOGI(TAG,  "MqttPortCallbacks.onWrite: %d", config.mqttPort);      
  }
};

class CmdCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) {
    std::string value = characteristic->getValue();
    Serial.printf("BLE command=%s\n", value.c_str());
    if (!strcmp(value.c_str(), BLE_CMD_BLE_EXIT)) {
      // mark unit for software reboot (exit BLE mode)
      if (dirty) {
        EEPROM.put(0, config);
        EEPROM.commit();
        ESP_LOGI(TAG, "config saved in flash memory:");        
        if (RUNTIME_LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE) {
          dumpConfig(&config);
        }
      } else {
        Serial.println("config save skipped - nothing modified");
      }
      bleMode = 0;
      swReboot = 1;
    } else if (!strcmp(value.c_str(), BLE_CMD_BLE_START)) {
      // mark unit for software reboot (start BLE mode)
      bleMode++;
      swReboot = 2;
    } else {
      ESP_LOGE(TAG, "BLE command not recognized: %s\n", value.c_str());      
    }
  }
};


class HubCertCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) {
    std::string value = characteristic->getValue();
    dirty = true;
    if (value == "clear") {
      config.thingCrt[0] = 0;
    } else {
      if (strlen(config.thingCrt) + strlen(value.c_str()) < sizeof(config.thingCrt) - 1) {  // don't need to do partial copy if too long; partial cert doesn't have any use
        strcat(config.thingCrt, value.c_str());
      }
    }
    //Serial.println(strlen(config.thingCrt));
    //Serial.printf("HubCertCallbacks: %d %s", strlen(config.thingCrt), config.thingCrt);
    //Serial.printf("HubCertCallbacks: current length=%d\n", strlen(config.thingCrt));
    ESP_LOGI(TAG, "HubCertCallbacks: current length=%d\n", strlen(config.thingCrt));
  }
};


class HubKeyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) {
    std::string value = characteristic->getValue();
    dirty = true;
    if (value == "clear") {
      config.thingPrivateKey[0] = 0;
    } else {
      if (strlen(config.thingPrivateKey) + strlen(value.c_str()) < sizeof(config.thingPrivateKey) - 1) {  // don't need to do partial copy if too long; partial key doesn't have any use
        strcat(config.thingPrivateKey, value.c_str());
      }
    }
    //Serial.println(strlen(config.thingPrivateKey));
    //Serial.printf("HubKeyCallbacks: current length=%d\n", strlen(config.thingPrivateKey));
    ESP_LOGI(TAG, "HubKeyCallbacks: current length=%d\n", strlen(config.thingPrivateKey));
  }
};


void startBLE() {

  String bleName = "Sensaurus-";
  bleName += config.hubId;
  BLEDevice::init(bleName.c_str());
  BLEServer *server = BLEDevice::createServer();
  BLECharacteristic *characteristic;
  
  // create and start core service and characteristics
  BLEService *service = server->createService(BLE_SERVICE_UUID);
  
  
  // add characteristics
  characteristic = service->createCharacteristic(WIFI_NETWORK_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  characteristic->setCallbacks(new WifiNetworkCallbacks());
  characteristic->setValue(config.wifiNetwork);
  characteristic = service->createCharacteristic(WIFI_PASSWORD_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  characteristic->setCallbacks(new WifiPasswordCallbacks());
  characteristic->setValue(config.wifiPassword);
  characteristic = service->createCharacteristic(OWNER_ID_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  characteristic->setCallbacks(new OwnerIdCallbacks());
  characteristic->setValue(config.ownerId);
  characteristic = service->createCharacteristic(HUB_ID_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  characteristic->setCallbacks(new HubIdCallbacks());
  characteristic->setValue(config.hubId);  
  characteristic = service->createCharacteristic(BLE_CMD_UUID, BLECharacteristic::PROPERTY_WRITE);
  characteristic->setCallbacks(new CmdCallbacks());

  
  service->start();

  // create and start mqtt service and characteristics  
  BLEService *service2 = server->createService(BLE_SERVICE_MQTT_UUID);
  
  characteristic = service2->createCharacteristic(MQTT_USER_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  characteristic->setCallbacks(new MqttUserCallbacks());
  characteristic->setValue(config.mqttUser);
  characteristic = service2->createCharacteristic(MQTT_PASSWORD_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  characteristic->setCallbacks(new MqttPasswordCallbacks());
  characteristic->setValue(config.mqttPassword);
  characteristic = service2->createCharacteristic(MQTT_SERVER_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  characteristic->setCallbacks(new MqttServerCallbacks());
  characteristic->setValue(config.mqttServer);
  characteristic = service2->createCharacteristic(MQTT_PORT_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  characteristic->setCallbacks(new MqttPortCallbacks());
  characteristic->setValue(config.mqttPort);  
  characteristic = service2->createCharacteristic(HUB_CERT_UUID, BLECharacteristic::PROPERTY_WRITE);
  characteristic->setCallbacks(new HubCertCallbacks());
  characteristic = service2->createCharacteristic(HUB_KEY_UUID, BLECharacteristic::PROPERTY_WRITE);
  characteristic->setCallbacks(new HubKeyCallbacks());
  
  service2->start();

  
  BLEAdvertising *advertising = server->getAdvertising();
  
  advertising->addServiceUUID(BLE_SERVICE_UUID);
  advertising->addServiceUUID(BLE_SERVICE_MQTT_UUID);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  advertising->setMinPreferred(0x12);
  advertising->start();
  
  ESP_LOGI(TAG, "startBLE: Started advertising for %s", bleName.c_str());
  
}


#endif  // ENABLE_BLE
