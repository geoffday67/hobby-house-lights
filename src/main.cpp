#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define SECURITY_PIN    5

#define RED_PIN         14  // D5
#define GREEN_PIN       12  // D6
#define BLUE_PIN        13  // D7

#define MQTT_SERVER       "192.168.1.5"
#define MQTT_PORT         1883
#define MQTT_CLIENT       "hobby-house"
#define SECURITY_TOPIC    "hobby/security"
#define LIGHTS_TOPIC      "hobby/lights"

WiFiClient wifi;
PubSubClient mqtt(wifi);

// Current values as set via MQTT
int red = 255, green = 255, blue = 255, brightness = 50;
bool active = false, enabled = true;

void switchOnSecurity(bool on) {
  if (on) {
    digitalWrite(SECURITY_PIN, HIGH);
  } else {
    digitalWrite(SECURITY_PIN, LOW);
  }
}

// Show lights according to parameters set via MQTT
void showLights() {
  // If not enabled then just turn them off
  if (!enabled) {
    analogWrite(RED_PIN, 0);
    analogWrite(GREEN_PIN, 0);
    analogWrite(BLUE_PIN, 0);
    return;
  }

  // They're enabled, check for 'active'. turn them on or off accordingly
  if (active) {
    analogWrite (RED_PIN, red * brightness / 100);
    analogWrite (GREEN_PIN, green * brightness / 100);
    analogWrite (BLUE_PIN, blue * brightness / 100);
  } else {
    analogWrite(RED_PIN, 0);
    analogWrite(GREEN_PIN, 0);
    analogWrite(BLUE_PIN, 0);
  }
}

void onMessage(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<200> json;

  DeserializationError error = deserializeJson (json, payload, length);
  if (error) {
    Serial.println("Error");
    Serial.println(error.c_str());
    return;
  }

 // Get security setting if present
  if (!strcmp(topic, SECURITY_TOPIC)) {
    const char *pstate = json["state"];
    if (pstate) {
      // Got a security setting, act on it
      Serial.print("Received security ");
      Serial.println(pstate);

      if (!strcmp(pstate, "on")) {
        switchOnSecurity(true);
      } else if (!strcmp(pstate, "off")) {
        switchOnSecurity(false);
      }
    }

    return;
  }

  // Get outside lights setting if present
  if (!strcmp(topic, LIGHTS_TOPIC)) {
    JsonVariant colour = json["colour"];
    if (!colour.isNull()) {
      red = colour["red"];
      green = colour["green"];
      blue = colour["blue"];

      Serial.print("Colour: red = ");
      Serial.print(red);
      Serial.print(", green = ");
      Serial.print(green);
      Serial.print(", blue = ");
      Serial.println(blue);
    }

    if (json.containsKey("brightness")) {
      brightness = json["brightness"];

      Serial.print("Brightness: ");
      Serial.println(brightness);
    }

    if (json.containsKey("active")) {
      active = json["active"];

      Serial.print("Active: ");
      Serial.println(active ? "true" : "false");
    }

    if (json.containsKey("enabled")) {
      active = json["enabled"];

      Serial.print("Enabled: ");
      Serial.println(enabled ? "true" : "false");
    }

    showLights();
    return;
  }
}

void connectWiFi() {
  // Check for stored credentials and use them if found
  if (WiFi.SSID().length() != 0) {
    Serial.print ("Connecting with existing parameters ");
    WiFi.reconnect();
  } else {
    int warioStrength = -999;
    int hobbyHouseStrength = -999;
    Serial.println("Scanning for networks");
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++)
    {
      if (WiFi.SSID(i) == "Wario") {
        warioStrength = WiFi.RSSI(i);
      }

      if (WiFi.SSID(i) == "HobbyHouse") {
        hobbyHouseStrength = WiFi.RSSI(i);
      }
    }

    if (warioStrength > hobbyHouseStrength) {
      WiFi.begin("Wario", "mansion1");
      Serial.print("Connecting to Wario ");
    } else {
      WiFi.begin("HobbyHouse", "mansion1");
      Serial.print("Connecting to HobbyHouse ");
    }
  }

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  if (mqtt.connect(MQTT_CLIENT)) {
    mqtt.subscribe(SECURITY_TOPIC);
    mqtt.subscribe(LIGHTS_TOPIC);
    Serial.println("MQTT connected and subscribed");
  } else {
    Serial.print("Error connecting to MQTT ");
    Serial.println(mqtt.state());
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  randomSeed(micros());

  analogWriteRange(255);
  analogWriteFreq(10000);

  pinMode(SECURITY_PIN, OUTPUT);
  switchOnSecurity(true);

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  showLights();

  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();  // Force re-scan by clearing stored credentials
  delay(100);
  connectWiFi();

  // Initialise MQTT
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(onMessage);
  Serial.println("MQTT started");
  connectMQTT();
}

void loop() {
  mqtt.loop();

  if (!mqtt.connected()) {
    Serial.println("MQTT disconnected");
    mqtt.disconnect();
    connectWiFi();
    connectMQTT();
  }
}