#include <Arduino.h>
#include <ArduinoJson.h>
#include <aliyun_mqtt.h>
#include <WiFi.h>

// https://techtutorialsx.com/2017/06/29/esp32-arduino-getting-started-with-wifi/
#define WIFI_SSID "TP-LINK_BC67"
#define WIFI_PASSWD "liu88837818"


#define PRODUCT_KEY             "a11uHDWlyw3"
#define DEVICE_NAME             "esp32Light"
#define DEVICE_SECRET           "aSEImRqGBNOUzijDUHDi1suF3Ai6grk1"

#define ALINK_BODY_FORMAT "{\"id\":\"%u\",\"version\":\"1.0\",\"method\":\"%s\",\"params\":%s}"
#define ALINK_TOPIC_PROP_POST "/sys/" PRODUCT_KEY "/" DEVICE_NAME "/thing/event/property/post"
#define ALINK_TOPIC_PROP_SET "/sys/" PRODUCT_KEY "/" DEVICE_NAME "/thing/service/property/set"
#define ALINK_METHOD_PROP_POST "thing.event.property.post"

#define LED 2

#define LED_OFF 0
#define LED_ON 1
int ledState = LED_OFF;
bool needReportStatus = true;
int ledStateMapOutput[2] = {HIGH, LOW}; // ESP32Devkitc built-in LED output flip
// int ledStateMapOutput[2] = {LOW, HIGH}; // FireBeetle built-in LED output flip

unsigned long lastMqttConnectMs = 0;

unsigned int postMsgId = 0;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

String translateEncryptionType(wifi_auth_mode_t encryptionType) {

  switch (encryptionType) {
    case (WIFI_AUTH_OPEN):
      return "Open";
    case (WIFI_AUTH_WEP):
      return "WEP";
    case (WIFI_AUTH_WPA_PSK):
      return "WPA_PSK";
    case (WIFI_AUTH_WPA2_PSK):
      return "WPA2_PSK";
    case (WIFI_AUTH_WPA_WPA2_PSK):
      return "WPA_WPA2_PSK";
    case (WIFI_AUTH_WPA2_ENTERPRISE):
      return "WPA2_ENTERPRISE";
  }
}

void scanNetworks() {

  int numberOfNetworks = WiFi.scanNetworks();

  Serial.print("Number of networks found: ");
  Serial.println(numberOfNetworks);

  for (int i = 0; i < numberOfNetworks; i++) {

    Serial.print("Network name: ");
    Serial.println(WiFi.SSID(i));

    Serial.print("Signal strength: ");
    Serial.println(WiFi.RSSI(i));

    Serial.print("MAC address: ");
    Serial.println(WiFi.BSSIDstr(i));

    Serial.print("Encryption type: ");
    String encryptionTypeDescription = translateEncryptionType(WiFi.encryptionType(i));
    Serial.println(encryptionTypeDescription);
    Serial.println("-----------------------");

  }
}

void connectToNetwork() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Establishing connection to WiFi..");
  }

  Serial.println("Connected to network.");

}

void mqttCheckConnect()
{
  bool connected = connectAliyunMQTT(mqttClient, PRODUCT_KEY, DEVICE_NAME, DEVICE_SECRET);
  if (connected) {
    Serial.println("MQTT connect succeed!");
    if (mqttClient.subscribe(ALINK_TOPIC_PROP_SET)) {
      Serial.println("subscribe done.");
    } else {
      Serial.println("subscribe failed!");
    }
  }
}

void mqttPublish()
{
  char param[32];
  char jsonBuf[128];

  sprintf(param, "{\"LightSwitch\":%d}", ledState);
  postMsgId += 1;
  sprintf(jsonBuf, ALINK_BODY_FORMAT, postMsgId, ALINK_METHOD_PROP_POST, param);

  if (mqttClient.publish(ALINK_TOPIC_PROP_POST, jsonBuf)) {
     Serial.print("Post message to cloud: ");
     Serial.println(jsonBuf);
  } else {
    Serial.println("Publish message to cloud failed!");
  }
}

// https://pubsubclient.knolleary.net/api.html#callback
void callback(char* topic, byte* payload, unsigned int length)
{
  if (strstr(topic, ALINK_TOPIC_PROP_SET))
  {
    Serial.print("Set message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    payload[length] = '\0';
    Serial.println((char *)payload);

    // Deserialization break change from 5.x to 6.x of ArduinoJson
    DynamicJsonDocument doc(100);
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      Serial.println("parse json failed");
      return;
    }

    // {"method":"thing.service.property.set","id":"282860794","params":{"LightSwitch":1},"version":"1.0.0"}
    JsonObject setAlinkMsgObj = doc.as<JsonObject>();
    // LightSwitch
    int desiredLedState = setAlinkMsgObj["params"]["LightSwitch"];

    if (desiredLedState == LED_ON || desiredLedState == LED_OFF) {
      needReportStatus = true;
      ledState = desiredLedState;

      const char* cmdStr = desiredLedState == LED_ON ? "on" : "off";
      Serial.print("Cloud command: Turn ");
      Serial.print(cmdStr);
      Serial.println(" the light.");
    }
  }
}

void setup()
{
    Serial.begin(115200);

    pinMode(LED, OUTPUT);

    scanNetworks();
    connectToNetwork();

    Serial.println(WiFi.macAddress());
    Serial.println(WiFi.localIP());

    // WiFi.disconnect(true);
    // Serial.println(WiFi.localIP());
    mqttClient.setCallback(callback);

    lastMqttConnectMs = millis();
    mqttCheckConnect();
}

void loop()
{
    if (millis() - lastMqttConnectMs >= 5000) {
      lastMqttConnectMs = millis();
      mqttCheckConnect();
    }

    // https://pubsubclient.knolleary.net/api.html#loop
    if (!mqttClient.loop()) {
      Serial.println("The MQTT client is disconnected!");
    }

    digitalWrite(LED, ledStateMapOutput[ledState]);
    if (needReportStatus) {
      mqttPublish();
      needReportStatus = false;
    }
}