/*

  Copyright (c) 2017 @KmanOz
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  ==============================================================================
  Changes in v1.01tOTA
  
    - Relay state now stored in EEPROM and will power up with last relay state
    - OTA Firmware Upgradable
  ==============================================================================

  **** USE THIS Firmware for: Sonof TH10/16 fitted with AM2303 / DHT22 supplied by ITEAD ****

*/

#include <EEPROM.h>
#include "DHT.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <Ticker.h>

#define BUTTON          0                                    // (Don't Change for Sonoff TH Series)
#define RELAY           12                                   // (Don't Change for Sonoff TH Series)
#define LED             13                                   // (Don't Change for Sonoff TH Series)
#define DHTPIN          14                                   // (Don't Change for Sonoff TH Series)

#define DHTTYPE         DHT22                                // (11 or 22) DHT Type

#define MQTT_CLIENT     "Sonoff_Living_Room_v1.01tOTA"       // mqtt client_id (Must be unique for each Sonoff)
#define MQTT_SERVER     "192.168.0.100"                      // mqtt server
#define MQTT_PORT       1883                                 // mqtt port
#define MQTT_TOPIC      "home/sonoff/living_room/1"          // mqtt topic (Must be unique for each Sonoff)
#define MQTT_USER       "user"                               // mqtt user
#define MQTT_PASS       "pass"                               // mqtt password

#define WIFI_SSID       "homewifi"                           // wifi ssid
#define WIFI_PASS       "homepass"                           // wifi password

#define VERSION    "\n\n---------------  Sonoff TH Powerpoint v1.01tOTA  ---------------"

DHT dht(DHTPIN, DHTTYPE, 11);                                // (Don't Change for Sonoff TH Series)

bool rememberRelayState = true;                              // If 'true' remembers the state of the relay before power loss.
bool OTAupdate = false;                                      // (Do not Change)
bool requestRestart = false;                                 // (Do not Change)
bool sendStatus = false;                                     // (Do not Change)
bool tempReport = false;                                     // (Do not Change)

int kUpdFreq = 1;                                            // Update frequency in Mintes to check for mqtt connection
int kRetries = 10;                                           // WiFi retry count. Increase if not connecting to router.
int lastRelayState;                                          // (Do not Change)

unsigned long TTasks;                                        // (Do not Change)
unsigned long count = 0;                                     // (Do not Change)

extern "C" { 
  #include "user_interface.h" 
}

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient, MQTT_SERVER, MQTT_PORT);
Ticker btn_timer;

void callback(const MQTT::Publish& pub) {
  if (pub.payload_string() == "stat") {
  }
  else if (pub.payload_string() == "on") {
    digitalWrite(RELAY, HIGH);
  }
  else if (pub.payload_string() == "off") {
    digitalWrite(RELAY, LOW);
  }
  else if (pub.payload_string() == "reset") {
    requestRestart = true;
  }
  else if (pub.payload_string() == "temp") {
    tempReport = true;
  }
  sendStatus = true;
}

void setup() {
  pinMode(LED, OUTPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(BUTTON, INPUT);
  digitalWrite(LED, HIGH);
  digitalWrite(RELAY, LOW);
  Serial.begin(115200);
  EEPROM.begin(8);
  lastRelayState = EEPROM.read(0);
  if (rememberRelayState && lastRelayState == 1) {
     digitalWrite(RELAY, HIGH);
  }
  btn_timer.attach(0.05, button);
  mqttClient.set_callback(callback);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  ArduinoOTA.onStart([]() {
    OTAupdate = true;
    blinkLED(LED, 400, 2);
    digitalWrite(LED, HIGH);
    Serial.println("OTA Update Initiated . . .");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Update Ended . . .s");
    ESP.restart();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    digitalWrite(LED, LOW);
    delay(5);
    digitalWrite(LED, HIGH);
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    blinkLED(LED, 40, 2);
    OTAupdate = false;
    Serial.printf("OTA Error [%u] ", error);
    if (error == OTA_AUTH_ERROR) Serial.println(". . . . . . . . . . . . . . . Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println(". . . . . . . . . . . . . . . Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println(". . . . . . . . . . . . . . . Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println(". . . . . . . . . . . . . . . Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println(". . . . . . . . . . . . . . . End Failed");
  });
  ArduinoOTA.begin();
  Serial.println(VERSION);
  Serial.print("\nUnit ID: ");
  Serial.print("esp8266-");
  Serial.print(ESP.getChipId(), HEX);
  Serial.print("\nConnecting to "); Serial.print(WIFI_SSID); Serial.print(" Wifi"); 
  while ((WiFi.status() != WL_CONNECTED) && kRetries --) {
    delay(500);
    Serial.print(" .");
  }
  if (WiFi.status() == WL_CONNECTED) {  
    Serial.println(" DONE");
    Serial.print("IP Address is: "); Serial.println(WiFi.localIP());
    Serial.print("Connecting to ");Serial.print(MQTT_SERVER);Serial.print(" Broker . .");
    delay(500);
    while (!mqttClient.connect(MQTT::Connect(MQTT_CLIENT).set_keepalive(90).set_auth(MQTT_USER, MQTT_PASS)) && kRetries --) {
      Serial.print(" .");
      delay(1000);
    }
    if(mqttClient.connected()) {
      Serial.println(" DONE");
      Serial.println("\n----------------------------  Logs  ----------------------------");
      Serial.println();
      mqttClient.subscribe(MQTT_TOPIC);
      blinkLED(LED, 40, 8);
      digitalWrite(LED, LOW);
    }
    else {
      Serial.println(" FAILED!");
      Serial.println("\n----------------------------------------------------------------");
      Serial.println();
    }
  }
  else {
    Serial.println(" WiFi FAILED!");
    Serial.println("\n----------------------------------------------------------------");
    Serial.println();
  }
  getTemp();
}

void loop() { 
  ArduinoOTA.handle();
  if (OTAupdate == false) { 
    mqttClient.loop();
    timedTasks();
    checkStatus();
    if (tempReport) {
      getTemp();
    }
  }
}

void blinkLED(int pin, int duration, int n) {             
  for(int i=0; i<n; i++)  {  
    digitalWrite(pin, HIGH);        
    delay(duration);
    digitalWrite(pin, LOW);
    delay(duration);
  }
}

void button() {
  if (!digitalRead(BUTTON)) {
    count++;
  } 
  else {
    if (count > 1 && count <= 40) {   
      digitalWrite(RELAY, !digitalRead(RELAY));
      sendStatus = true;
    } 
    else if (count >40){
      Serial.println("\n\nSonoff Rebooting . . . . . . . . Please Wait"); 
      requestRestart = true;
    } 
    count=0;
  }
}

void checkConnection() {
  if (WiFi.status() == WL_CONNECTED)  {
    if (mqttClient.connected()) {
      Serial.println("mqtt broker connection . . . . . . . . . . OK");
    } 
    else {
      Serial.println("mqtt broker connection . . . . . . . . . . LOST");
      requestRestart = true;
    }
  }
  else { 
    Serial.println("WiFi connection . . . . . . . . . . LOST");
    requestRestart = true;
  }
}

void checkStatus() {
  if (sendStatus) {
    if(digitalRead(RELAY) == LOW)  {
      if (rememberRelayState) {
        EEPROM.write(0, 0);
      }
      mqttClient.publish(MQTT::Publish(MQTT_TOPIC"/stat", "off").set_retain().set_qos(1));
      Serial.println("Relay . . . . . . . . . . . . . . . . . . OFF");
    } else {
      if (rememberRelayState) {
        EEPROM.write(0, 1);
      }
      mqttClient.publish(MQTT::Publish(MQTT_TOPIC"/stat", "on").set_retain().set_qos(1));
      Serial.println("Relay . . . . . . . . . . . . . . . . . . ON");
    }
    if (rememberRelayState) {
      EEPROM.commit();
    }
    sendStatus = false;
  }
  if (requestRestart) {
    blinkLED(LED, 400, 4);
    ESP.restart();
  }
}

void getTemp() {
  Serial.print("DHT read . . . . . . . . . . . . . . . . . ");
  float dhtH, dhtT;
  char message_buff[60];
  dhtH = dht.readHumidity();
  dhtT = dht.readTemperature();
  if(digitalRead(LED) == LOW)  {
    blinkLED(LED, 100, 1);
  } else {
    blinkLED(LED, 100, 1);
    digitalWrite(LED, HIGH);
  }
  if (isnan(dhtH) || isnan(dhtT)) {
    mqttClient.publish(MQTT::Publish(MQTT_TOPIC"/debug","\"DHT Read Error\"").set_retain().set_qos(1));
    Serial.println("ERROR");
    tempReport = false;
    return;
  }
  String pubString = "{\"Temp\": "+String(dhtT)+", "+"\"Humidity\": "+String(dhtH) + "}";
  pubString.toCharArray(message_buff, pubString.length()+1);
  mqttClient.publish(MQTT::Publish(MQTT_TOPIC"/temp", message_buff).set_retain().set_qos(1));
  Serial.println("OK");
  tempReport = false;
}

void timedTasks() {
  if ((millis() > TTasks + (kUpdFreq*60000)) || (millis() < TTasks)) { 
    TTasks = millis();
    checkConnection();
    tempReport = true;
  }
}
