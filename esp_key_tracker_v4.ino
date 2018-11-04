#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
//#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>

#include <ArduinoOTA.h>

/*
HSBNE KeyTracker
================
Modified code from Jabelone and ideas from Nog3.

Object
======
To keep track of keys in the 'space. 

Method
======
Board is a Memos D1 Mini.
Key-rings are attached to 1/4" jack plugs (each containing a E12 resistor).
The s/w tracks which keys are present and shouts out to the world via..
QMTT topics : keytracker/removed, keytracker/returned, keytracker/status.
JSON collection : http://keytracker.local/status
Web page : http://keytracker.local/index.html

mDNS is used to publish hostname.

A0 is pulled up by a 3.9k resistor.
One side of each jack is attached to A0. The other end is attached to a digital pin.
Each key is a 1/4" mono jack with an E12 resistor in it.
Digital outputs are left floating.
In sequence, each digital pin is pulled to 0v and A0 is sampled.

Original calculations :

Key     R   Voltage Digital Delta   Key   ACTUAL!
key 1   12  2.49    772             Wood    731
key 2   6.8 2.10    650     122     Metal   618
key 3   3.9 1.65    512     138     Gate    488
key 4   1.8 1.04    323     189
key 5   1   0.67    208     115
key 6   0   0.00    0       208

These ideal values are off because I didn't consider that (a) A0 is actaully a potential devider itself (going from 3.3v to 1.0v) 
and (b) many digital pins have additional components hanging from them that skew the results (pull-ups/LEDs etc).

WiFi/Webserver Details
*/

const char* ssid =      "HSBNEAccess";
const char* password =  "cookiemonster";
#define MQTT_SERVER     "10.0.1.253"
#define OTA_HOSTNAME    "SENSE-keytracker"
#define OTA_PASSWORD    "cookiemonster"

/*
const char* ssid =        "OPTUS_4E5769";
const char* password =    "ma*******97895";
#define MQTT_SERVER       "192.168.0.69"
*/

const int server_port = 80;

#define MQTT_SERVERPORT  1883                   // use 8883 for SSL
#define MQTT_USERNAME    "keytracker"
#define MQTT_PASSWORD    "keytracker123"

#define MQTT_CHAT_TOPIC      "keytracker/chat"
#define MQTT_REMOVED_TOPIC   "keytracker/removed"
#define MQTT_RETURNED_TOPIC  "keytracker/returned"
#define MQTT_STATUS_TOPIC    "keytracker/status"

#define REMOVEDINTERVAL 10000 // how frequently to poll keys (mS)
#define STATUSINTERVAL  600000 // how frequently to post status (mS)

const String monitoredDeviceNames[] = {"Woodshop key", "MetalShop key", "Gate key", "LPG cage key", "Key 5"};
const int monitoredDevicePins[] = {D1, D2, D5, D6, D7}; 
const int analogPin = A0;
const int keyVoltage[] = {800, 670, 530, 330, 208, 0}; // Corrected 3/11/18
const int monitoredDevices = 4;
const int monitoredInputs = 5;
long deviceRemoved[] = {0,0,0,0,0,0}; // when (in millis) a key was removed.
long checkRemoved = 0;
long checkStatus = 0;
long mqttLastRetry = 0;
long now;
int key;

// Program variables

ESP8266WebServer server(server_port);
WiFiClient espClient;
PubSubClient MQTTClient(espClient);

void setup() {
  delay(5000); // in case I need to unplug/replug to reset.
  
  Serial.begin(9600); // Start serial
  delay(500);
  Serial.println("Wemo KeyTracker v4");
  Serial.println("==================\n");
  Serial.println("Programmed with OTA!");

  Serial.println("\nConnecting to: " + (String)ssid);
  WiFi.begin(ssid, password); // Attempt a WiFi connection in station mode

  // Let's wait until WiFi is connected before anything else
  while (WiFi.status() != WL_CONNECTED) {
    delay(5000);
    Serial.print(".");
  }

  // We're connected to WiFi so let's print some info
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  delay(5000);
  
  // Setup MQTT client..
  Serial.println("Set MQTT server.");
  MQTTClient.setServer(MQTT_SERVER, MQTT_SERVERPORT);

  // Handle the webserver requests
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/help", handleHelp);
  server.onNotFound(handleNotFound);
  Serial.println("Start web server.");
  server.begin();

  Serial.println("\nSetting up pins.");

  // Set up all our pins to open-collector input state.
  for (int x = 0; x < monitoredInputs; x++) {
    pinMode(monitoredDevicePins[x], INPUT);
  }
  delay(1000);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  Serial.println("\nStarting OTA.");
  
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();

  Serial.println("\n~~~==== Setup complete ====~~~");

}

void loop() {
  now = millis();
  ArduinoOTA.handle();
  // try to reconnect to mqtt server every 30s. Non-blocking.
  if (!MQTTClient.connected() and (now-mqttLastRetry)>30000 ) {
      mqttLastRetry = now;
      Serial.println("Attempt MQTT reconnect.");
      mqttReconnect();
    }

  if (MQTTClient.connected()) MQTTClient.loop(); // Handle MQTT events
  
  server.handleClient(); // Handle web server requests if necessary

  if (now > checkRemoved) { // Update 'deviceRemoved' array.
    for (int i=0; i<monitoredDevices; i++) {  // mark all keys as removed now. Timestamp on keys already removed not touched.
      if (deviceRemoved[i]==0) deviceRemoved[i]=now;
      }
    for (int i=0; i<monitoredInputs; i++) {  // if we find the key, set 'deviceRemoved' to 0.
      key = keyOnInput(i);
      if (key>=0) { // found a key..
        if (deviceRemoved[key]!=now) announceKeyReturned(monitoredDeviceNames[key]);  // if this key was previously removed, announce its return.
        deviceRemoved[key]=0;
      }
    }
    
    // At this point, newly removed keys have 'now' as a timestamp, returned keys have 0 as a timestamp, previously removed keys have an earlier timestamp.
    
    for (int i=0; i<monitoredDevices; i++) {
      if (deviceRemoved[i]==now) announceKeyRemoved(monitoredDeviceNames[i]);
    }
  checkRemoved=millis()+REMOVEDINTERVAL;
  }

  if (now > checkStatus) { // Update status.
    announceKeyStatus(asJson());
    checkStatus=millis()+STATUSINTERVAL;
  }
  
}

//========================================================================================================================


String asJson() {
  String result;
  long removedFor;
  result="{";
  for (int i=0; i<monitoredDevices; i++) {
    removedFor=0;
    if (deviceRemoved[i]>0) removedFor=(now - deviceRemoved[i])/1000;
    result+="\""+monitoredDeviceNames[i]+"\":"+String(removedFor);
    if (i<monitoredDevices-1) result+=",";
    }
   result+="}";
   return result;
  }
  
void mqttReconnect() {
  // Loop until we're reconnected
//  while (!MQTTClient.connected()) {
    Serial.println("\nAttempting MQTT connection...");
    if (MQTTClient.connect("KeyTracker", MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("connected");
      MQTTClient.publish(MQTT_CHAT_TOPIC,  String("Key Tracker connected.").c_str(), true);
    } else {
      Serial.print("failed, rc=");
      Serial.println(MQTTClient.state());
    }
//  }
  Serial.println("Ho Hum");
}

int keyOnInput(int x) {
  // find which key is attched to an input. Return -1 if no key attached.
  int v; // return from ADC.
  int i; // counter
  int result; // err.. the result.

  for(i=0; i<monitoredInputs; i++) { // set all pins to o/c input. Should be redundant.
    pinMode(monitoredDevicePins[i], INPUT);
  }
  pinMode(monitoredDevicePins[x], OUTPUT);
  digitalWrite(monitoredDevicePins[x], LOW); // set our pin to 0v.
  delay(20); // allow ADC to settle.
  v = analogRead(analogPin);
  Serial.print("Input ");
  Serial.print(x);
  Serial.print(" ");
  Serial.println(v);
  
  pinMode(monitoredDevicePins[x], INPUT); // set our pin back to o/c input.
  
  result=-1;
  for(i=0; i<monitoredDevices; i++) { // run through expected voltages.
    if (keyVoltage[i]<(v+40) and keyVoltage[i]>(v-40)) result=i;  // allow a generous range of readings around the ideal expected value.
  }
  return(result);
}

void announceKeyRemoved(String message) {
  if (MQTTClient.connected()) {
  MQTTClient.publish(MQTT_REMOVED_TOPIC, message.c_str(), true);
  }
  Serial.print("Key Removed: ");
  Serial.println(message);
}

void announceKeyReturned(String message) {
  if (MQTTClient.connected()) {
  MQTTClient.publish(MQTT_RETURNED_TOPIC, message.c_str(), true);
  }
  Serial.print("Key Returned: ");
  Serial.println(message);
}

void announceKeyChat(String message) {
  if (MQTTClient.connected()) {
  MQTTClient.publish(MQTT_CHAT_TOPIC, message.c_str(), true);
  }
  Serial.print("Chat: ");
  Serial.println(message);
}

void announceKeyStatus(String message) {
  if (MQTTClient.connected()) {
  MQTTClient.publish(MQTT_STATUS_TOPIC, message.c_str(), true);
  }
  Serial.print("Status: ");
  Serial.println(message);
}

