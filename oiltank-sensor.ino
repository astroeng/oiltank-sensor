
#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include <WiFiClientSecure.h>
#include <limits.h>
#include <hcsr04.h>
#include <MQTT.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <time.h>

#include "mqtt-manager.h"
#include "config.h"
#include "secrets.h"

#define DEBUG(x) x

HCSR04 tank_sensor(PULSE, TRIG, SENSOR_INTERVAL);

const int MQTT_PORT = 8883;
const char MQTT_SUB_TOPIC[] = "$aws/things/" THINGNAME "/shadow/update";
const char MQTT_PUB_TOPIC[] = "$aws/things/" THINGNAME "/stream/update";

WiFiClientSecure netSecure;

MQTT_Manager aws_iot(THINGNAME, MQTT_HOST, MQTT_PORT, cacert, client_cert, privkey);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

unsigned long lastMillis = 0;

void sendData(unsigned long currentTime, double distance)
{
  DynamicJsonDocument jsonBuffer(256);
  JsonObject root = jsonBuffer.to<JsonObject>();
  JsonObject state = root.createNestedObject("state");
  JsonObject state_reported = state.createNestedObject("reported");
  state_reported["device_time"] = currentTime;
  state_reported["distance"] = distance;

  char shadow[measureJson(root) + 1];
  serializeJson(root, shadow, sizeof(shadow));

  Serial.println(shadow);

  if (!aws_iot.publish(MQTT_PUB_TOPIC, shadow, false, 0))
  {
    Serial.println(aws_iot.lastError());
  }
}

void checkTime()
{
  timeClient.update();
  struct timeval currentTime;
  currentTime.tv_sec = timeClient.getEpochTime();
  currentTime.tv_usec = 0;
  struct timezone currentZone;
  currentZone.tz_minuteswest = 0;
  currentZone.tz_dsttime = 0;
  settimeofday(&currentTime, &currentZone);
}

bool checkMQTT()
{
  if (!aws_iot.connected())
  {
    if(aws_iot.connect())
    {
      Serial.println("AWS-IOT Connected");
      return true;
    }
    else
    {
      char buffer[128];
      Serial.println("AWS-IOT Connection Failure");
      Serial.println(String(aws_iot.returnCode()) + " " +
                     String(aws_iot.lastError()) + " " +
                     String(netSecure.getLastSSLError(buffer, 127)));
      Serial.println(buffer);

    }
    return false;
  }
  return true;
}

void checkWifi()
{
  int state = 1;
  while (WiFi.status() != WL_CONNECTED)
  {
    // During the wait blink the status LED rapidly.
    digitalWrite(STATUS_LED, state);
    state ^= 1;
    delay(100);
  }
}

void setupWifi()
{
  WiFi.hostname(THINGNAME);
  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, pass);

  checkWifi();
}

void setup_serial()
{
  Serial.begin(115200);
}

// Dumb hack to get around std::bind issue/misunderstanding.

void tankISR()
{
  tank_sensor.pulseISR();
}

void msgHandler(String& topic, String& payload)
{
  Serial.println(topic + " " + payload);
  aws_iot.msgHandler(topic, payload);
}

void printPayload(String& payload)
{
  Serial.println(payload);
}

void setup()
{
  DEBUG(setup_serial());
  Serial.println("Setup - Started " + String(millis()));

  pinMode(STATUS_LED, OUTPUT);

  setupWifi();

  timeClient.begin();
  checkTime();

  tank_sensor.begin(tankISR);

  aws_iot.begin(netSecure, msgHandler);
  aws_iot.on(MQTT_SUB_TOPIC, printPayload);

  checkMQTT();

  Serial.println("Setup - Complete " + String(millis()));
}

void loop()
{

  // Update time on the device.
  checkTime();

  // Run the tank logic.
  tank_sensor.loop(20.0);

  checkWifi();

  if (checkMQTT())
  {
    aws_iot.loop();
    if (millis() - lastMillis > 10*60*1000)
    {
      Serial.println("AWS - IOT " + String(millis()));
      digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
      lastMillis += 10*60*1000;
      sendData(timeClient.getEpochTime(), tank_sensor.getDistance());
    }
  }
}
