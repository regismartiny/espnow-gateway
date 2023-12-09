#include <Arduino.h>
#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif
#include <espnow.h>
#include <WiFiClient.h>
#include "EspMQTTClient.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "secrets.h"


#define ENABLE_MQTT
#define ENABLE_NTP

#define MAX_MESSAGE_LENGTH 240

#define MQTT_MAX_PACKET_SIZE_              2048
#define DOMOTICZ_TOPIC_SEND                "domoticz/in"
#define DOMOTICZ_TOPIC_LAST_WILL           "domoticz/in/lastwill"
#define DOMOTICZ_TOPIC_LOG                 "domoticz/in/log"
#define DOMOTICZ_TOPIC_REQUEST_LOG         "domoticz/out/log"
#define DOMOTICZ_TOPIC_CMD                 "domoticz/in/cmd"

//Week Days
String weekDays[7]={"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

//Month names
String months[12]={"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};


enum msgType : uint {
  SENSOR_INFO = 1,
  LOG = 2,
  COMMAND = 3
};
static const char * msgTypeStrings[] = { "SENSOR_INFO", "LOG", "COMMAND" };

void publishMessage(const char* message, msgType type);
void printTime();


const char * getTextForEnum(int enumVal)
{
  return msgTypeStrings[enumVal-1];
}
// Structure example to receive data
// Must match the sender structure
typedef struct struct_message {
  char content[MAX_MESSAGE_LENGTH];
  msgType type;
  int page;
} struct_message;

// Create a struct_message called myData
struct_message myData;

const char* getTopicFromMsgType(msgType type) {
  switch (type)
  {
  case SENSOR_INFO:
    return DOMOTICZ_TOPIC_SEND;
    break;
  case LOG:
    return DOMOTICZ_TOPIC_LOG;
    break;
  case COMMAND:
    return DOMOTICZ_TOPIC_CMD;
    break;
  default:
    return "";
  }
}

void OnESPNowDataRecv(u8 * mac, u8 *incomingData, u8 len) {
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.println("---- ESP-NOW - New message received ----");
  #ifdef ENABLE_NTP
  printTime();
  #endif
  Serial.printf("Bytes received: %d\n", len);
  Serial.println("--Start message data");
  Serial.printf("length: %d\n", strlen(myData.content));
  Serial.printf("content: %s\n", myData.content);
  Serial.printf("type: %i - %s\n", myData.type, getTextForEnum(myData.type));
  Serial.printf("page: %d\n", myData.page);
  Serial.println("--End message data\n----");
  #ifdef ENABLE_MQTT
  publishMessage(myData.content, myData.type);
  #endif
}

void espNowInit() {
  Serial.println("Initializing ESP-NOW...");

  if (esp_now_init() != ERR_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(OnESPNowDataRecv);

  Serial.println("ESP-NOW initialized");
}

#ifdef ENABLE_NTP
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

void ntpTimeInit() {
  // Initialize a NTPClient to get time
  timeClient.begin();

  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  // GMT -3 = -10800
  timeClient.setTimeOffset(-10800);
}

void printTime() {
  String formattedTime = timeClient.getFormattedTime();
  Serial.print("Time: ");
  Serial.println(formattedTime);
}
#endif

#ifdef ENABLE_MQTT
EspMQTTClient client(WIFI_SSID, WIFI_PASSWORD, MQTT_SERVER, MQTT_USER, MQTT_PASSWORD, "ESPNOW-MQTT-GATEWAY", MQTT_PORT);

void publishMessage(const char* message, msgType type) {
  const char* topic = getTopicFromMsgType(type);
  client.publish(topic, message);
}

void mqttInit()
{
  // Optional functionalities of EspMQTTClient
  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridded with enableHTTPWebUpdater("user", "password").
  client.enableOTA(); // Enable OTA (Over The Air) updates. Password defaults to MQTTPassword. Port is the default OTA port. Can be overridden with enableOTA("password", port).
  client.enableLastWillMessage(DOMOTICZ_TOPIC_LAST_WILL, "going offline");  // You can activate the retain flag by setting the third parameter to true
  client.setMaxPacketSize(MQTT_MAX_PACKET_SIZE_);
}

// This function is called once everything is connected (Wifi and MQTT)
// WARNING : YOU MUST IMPLEMENT IT IF YOU USE EspMQTTClient
void onConnectionEstablished()
{
  espNowInit();
  // client.subscribe(DOMOTICZ_TOPIC_REQUEST_LOG, [](const String& payload) {
  //    LOGI("Log request message received");
  //   publishLogContent();
  // });

  Serial.println("Starting MQTT tests");

  // Subscribe to "mytopic/wildcardtest/#" and display received message to Serial
  client.subscribe("domoticz/out/#", [](const String & topic, const String & payload) {
    Serial.println("(From wildcard) topic: " + topic + ", payload: " + payload);
  });

  client.publish("domoticz/in/test", "This is a message from ESPNOW-MQTT-GATEWAY"); // You can activate the retain flag by setting the third parameter to true

  // Execute delayed instructions
  client.executeDelayed(5 * 1000, []() {
    client.publish("domoticz/out/test123", "This is a message from ESPNOW-MQTT-GATEWAY sent 5 seconds later");
  });
}
#endif

void setup() {
  // Initialize Serial Monitor
  Serial.begin(9600);

  WiFi.mode(WIFI_STA);
  Serial.printf("Wifi MAC address: %s\n", WiFi.macAddress().c_str());

  #ifdef ENABLE_MQTT
  mqttInit();
  #endif

  #ifdef ENABLE_NTP
  ntpTimeInit();
  #endif

  #ifndef ENABLE_MQTT
  espNowInit();
  #endif
}
 
void loop() {
  #ifdef ENABLE_MQTT
  client.loop();
  #endif
  #ifdef ENABLE_NTP
  timeClient.update();
  #endif
}