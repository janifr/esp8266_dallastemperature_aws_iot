#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "Secrets.h"
#include <DallasTemperature.h>
 
#define ONE_WIRE_BUS D2 // DS18B20 sensor
 
OneWire oneWire(ONE_WIRE_BUS);
 
DallasTemperature DS18B20(&oneWire);
DeviceAddress a;
 
float temp;

unsigned long lastMillis = 0;
const long interval = 5000;
 
#define AWS_IOT_PUBLISH_TOPIC   "esp8266_1/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp8266_1/sub"
 
WiFiClientSecure net;
 
BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);
 
PubSubClient client(net);
 
time_t now;
time_t nowish = 1510592825;
 
 
void NTPConnect(void)
{
  Serial.print("Setting time using SNTP");
  configTime(TIME_ZONE * 3600, 0 * 3600, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < nowish)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}
 
 
void messageReceived(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}
 
 
void connectAWS()
{
  delay(3000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  Serial.println(String("Attempting to connect to SSID: ") + String(WIFI_SSID));
 
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
 
  NTPConnect();
 
  net.setTrustAnchors(&cert);
  net.setClientRSACert(&client_crt, &key);
 
  client.setServer(MQTT_HOST, 8883);
  client.setCallback(messageReceived);
 
 
  Serial.println("Connecting to AWS IOT");
 
  while (!client.connect(THINGNAME))
  {
    Serial.print(".");
    delay(1000);
  }
 
  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }
  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
 
  Serial.println("AWS IoT Connected!");
}
 
 
void publishMessage()
{
  StaticJsonDocument<500> doc;
  DS18B20.requestTemperatures();
  uint8_t count = DS18B20.getDS18Count();
  char str[40];
  
  doc["time"] = time(nullptr);
  doc["sensors"] = count;
  for (int i = 0; i < count ; i++){
    temp = DS18B20.getTempCByIndex(i);
    if (DS18B20.getAddress(a, i))
      sprintf(str, "temperature_%02X%02X%02X%02X%02X%02X%02X%02X", a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7]);
    else
      sprintf(str, "temperature_%d", i);
    doc[str] = serialized(String(temp,2));
  }
  /*temp = DS18B20.getTempCByIndex(0);
  doc["temperature"] = temp;*/
  
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client
 
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}
 
 
void setup()
{
  Serial.begin(115200);
  connectAWS();
  DS18B20.begin(); 
}
 
 
void loop()
{
  delay(2000);
 
  now = time(nullptr);
 
  if (!client.connected())
  {
    connectAWS();
  }
  else
  {
    client.loop();
    if ((millis() - lastMillis > 300000) || (lastMillis == 0))
    {
      lastMillis = millis();
      publishMessage();
    }
  }  
}
