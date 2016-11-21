/**
   Workshop example for periodically sending temperature data.

   Visit https://www.losant.com/kit for full instructions.

   Copyright (c) 2016 Losant IoT. All rights reserved.
   https://www.losant.com
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Losant.h>

// WiFi credentials.
const char* WIFI_SSID = "my-wifi-ssid";
const char* WIFI_PASS = "my-wifi-pass";

// Losant credentials.
const char* LOSANT_DEVICE_ID = "my-device-id";
const char* LOSANT_ACCESS_KEY = "my-access-key";
const char* LOSANT_ACCESS_SECRET = "my-access-secret";

const char* LOSANT_ENABLE_TMP = "false";

const int BUTTON_PIN = 5;
const int LED_PIN = 4;

bool ledState = false;
bool deviceConfigured = false;

WiFiClientSecure wifiClient;

LosantDevice device;

void toggle() {
  Serial.println("Toggling LED.");
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState ? HIGH : LOW);
}

void handleCommand(LosantCommand *command) {
  Serial.print("Command received: ");
  Serial.println(command->name);

  if (strcmp(command->name, "toggle") == 0) {
    toggle();
  }
}

void connect() {
  // Connect to Wifi.
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  // WiFi fix: https://github.com/esp8266/Arduino/issues/2186
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    // Check to see if
    if (WiFi.status() == WL_CONNECT_FAILED) {
      Serial.println("Failed to connect to WIFI. Please verify credentials: ");
      Serial.println();
      Serial.print("SSID: ");
      Serial.println(WIFI_SSID);
      Serial.print("Password: ");
      Serial.println(WIFI_PASS);
      Serial.println();
      Serial.println("Trying again...");
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      delay(10000);
    }

    delay(500);
    Serial.println("...");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println();
  Serial.print("Connecting to Losant...");

  Serial.print("Authenticating Device...");
  HTTPClient http;
  http.begin("http://api.losant.com/auth/device");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");

  /* Create JSON payload to sent to Losant

       {
         "deviceId": "575ecf887ae143cd83dc4aa2",
         "key": "this_would_be_the_key",
         "secret": "this_would_be_the_secret"
       }

  */

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["deviceId"] = LOSANT_DEVICE_ID;
  root["key"] = LOSANT_ACCESS_KEY;
  root["secret"] = LOSANT_ACCESS_SECRET;
  String buffer;
  root.printTo(buffer);

  int httpCode = http.POST(buffer);

  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("This device is authorized!");
    } else {
      Serial.println("Failed to authorize device to Losant.");
      if (httpCode == 400) {
        Serial.println("Validation error: The device ID, access key, or access secret is not in the proper format.");
      } else if (httpCode == 401) {
        Serial.println("Invalid credentials to Losant: Please double-check the device ID, access key, and access secret.");
      } else {
        Serial.println("Unknown response from API");
      }
      Serial.println("Current Credentials: ");
      Serial.println("Device id: ");
      Serial.println(LOSANT_DEVICE_ID);
      Serial.println("Access Key: ");
      Serial.println(LOSANT_ACCESS_KEY);
      Serial.println("Access Secret: ");
      Serial.println(LOSANT_ACCESS_SECRET);
    }
  } else {
    Serial.println("Failed to connect to Losant API.");

  }

  http.end();

  device.setId(LOSANT_DEVICE_ID);
  device.connectSecure(wifiClient, LOSANT_ACCESS_KEY, LOSANT_ACCESS_SECRET);

  while (!device.connected()) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("Connected!");
  Serial.println("This device is now ready for use!");
}

void setup() {
  Serial.begin(115200);

  // Giving it a little time because the serial monitor doesn't
  // immediately attach. Want the workshop that's running to
  // appear on each upload.
  delay(2000);

  Serial.println();
  Serial.println("Running Workshop 3 Firmware. Waiting for config...");

  pinMode(BUTTON_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  device.onCommand(&handleCommand);
}

void buttonPressed() {
  Serial.println("Button Pressed!");
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["button"] = true;
  device.sendState(root);
}

void reportTemp(double degreesC, double degreesF) {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["tempC"] = degreesC;
  root["tempF"] = degreesF;
  device.sendState(root);
}

int buttonState = 0;
int currentRead = 0;
int timeSinceLastRead = 0;
int tempSum = 0;
int tempCount = 0;

void loop() {

  bool toReconnect = false;

  if (Serial.available() > 0) {
    Serial.println("");
    Serial.println("Recieved Serial input. Parseing as JSON: ");
    DynamicJsonBuffer jsonBuffer;
    String byteRead = Serial.readString();
    JsonObject& root = jsonBuffer.parseObject(byteRead);
    root.printTo(Serial);
    Serial.println();
    
    if (root.containsKey("losant-config-wifi-ssid")) {
      setConfig("losant-config-wifi-ssid", (const char*) root["losant-config-wifi-ssid"], WIFI_SSID);
    }

    if (root.containsKey("losant-config-wifi-pass")) {
      setConfig("losant-config-wifi-pass", (const char*) root["losant-config-wifi-pass"], WIFI_PASS);
    }

    if (root.containsKey("losant-config-device-id")) {
      setConfig("losant-config-device-id", (const char*) root["losant-config-device-id"], LOSANT_DEVICE_ID);
    }

    if (root.containsKey("losant-config-access-key")) {
      setConfig("losant-config-access-key", (const char*) root["losant-config-access-key"], LOSANT_ACCESS_KEY);
    }

    if (root.containsKey("losant-config-access-secret")) {
      setConfig("losant-config-access-secret", (const char*) root["losant-config-access-secret"], LOSANT_ACCESS_SECRET);
    }
    
    if (root.containsKey("losant-config-tmp")) {
      setConfig("losant-config-tmp", (const char*) root["losant-config-tmp"], LOSANT_ENABLE_TMP);
    }

    deviceConfigured = true;
    connect();
  }

  if (deviceConfigured != true) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Disconnected from WiFi");
    toReconnect = true;
  }

  if (!device.connected()) {
    Serial.println("Disconnected from MQTT");
    Serial.println(device.mqttClient.state());
    toReconnect = true;
  }

  if (toReconnect) {
    connect();
  }

  device.loop();

  currentRead = digitalRead(BUTTON_PIN);

  if (currentRead != buttonState) {
    buttonState = currentRead;
    if (buttonState) {
      buttonPressed();
    }
  }

  tempSum += analogRead(A0);
    tempCount++;

    // Report every 15 seconds.
    if (timeSinceLastRead > 15000) {
      // Take the average reading over the last 15 seconds.
      double raw = (double)tempSum / (double)tempCount;

      // The tmp36 documentation requires the -0.5 offset, but during
      // testing while attached to the Feather, all tmp36 sensors
      // required a -0.52 offset for better accuracy.
      double degreesC = (((raw / 1024.0) * 3.2) - 0.5) * 100.0;
      double degreesF = degreesC * 1.8 + 32;

      Serial.println();
      Serial.print("Temperature C: ");
      Serial.println(degreesC);
      Serial.print("Temperature F: ");
      Serial.println(degreesF);
      Serial.println();

      reportTemp(degreesC, degreesF);

      timeSinceLastRead = 0;
      tempSum = 0;
      tempCount = 0;
    }

    timeSinceLastRead += 100;

  delay(100);
}

void setConfig(String configName, const char* value, const char* &variable) {
  Serial.println();
  bool isEmpty = value ? 0 : 1;

  if (isEmpty) {
    Serial.println("ERROR Setting Config. Value is empty:  ");
    Serial.println(configName);
    return;
  }
  Serial.print("Setting ");
  Serial.print(configName);
  Serial.print(" to ");
  Serial.print(value);


  variable = value;
}
