#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing include/secrets.h. Create it and fill Wi-Fi/LED values."
#endif

namespace {
WebServer server(80);
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

uint8_t currentR = 0;
uint8_t currentG = 0;
uint8_t currentB = 0;
uint8_t currentBrightness = LED_BRIGHTNESS;
String currentTheme = "unknown";

void applyColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
  currentR = r;
  currentG = g;
  currentB = b;
  currentBrightness = brightness;

  strip.setBrightness(currentBrightness);
  strip.fill(strip.Color(currentR, currentG, currentB));
  strip.show();
}

void sendJsonResponse(int statusCode, const JsonDocument& doc) {
  String body;
  serializeJson(doc, body);
  server.send(statusCode, "application/json", body);
}

void handleHealth() {
  StaticJsonDocument<256> doc;
  doc["ok"] = true;
  doc["ip"] = WiFi.localIP().toString();
  doc["theme"] = currentTheme;
  doc["r"] = currentR;
  doc["g"] = currentG;
  doc["b"] = currentB;
  doc["brightness"] = currentBrightness;

  sendJsonResponse(200, doc);
}

void handleSetColor() {
  StaticJsonDocument<256> doc;

  if (!server.hasArg("plain")) {
    doc["ok"] = false;
    doc["error"] = "Expected JSON body";
    sendJsonResponse(400, doc);
    return;
  }

  StaticJsonDocument<256> input;
  DeserializationError error = deserializeJson(input, server.arg("plain"));
  if (error) {
    doc["ok"] = false;
    doc["error"] = "Invalid JSON";
    sendJsonResponse(400, doc);
    return;
  }

  int r = input["r"] | currentR;
  int g = input["g"] | currentG;
  int b = input["b"] | currentB;
  int brightness = input["brightness"] | currentBrightness;
  const char* theme = input["theme"] | "manual";

  r = constrain(r, 0, 255);
  g = constrain(g, 0, 255);
  b = constrain(b, 0, 255);
  brightness = constrain(brightness, 0, 255);

  currentTheme = theme;
  applyColor(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), static_cast<uint8_t>(brightness));

  doc["ok"] = true;
  doc["theme"] = currentTheme;
  doc["r"] = currentR;
  doc["g"] = currentG;
  doc["b"] = currentB;
  doc["brightness"] = currentBrightness;
  sendJsonResponse(200, doc);
}

void setupServer() {
  server.on("/health", HTTP_GET, handleHealth);
  server.on("/color", HTTP_POST, handleSetColor);
  server.onNotFound([]() {
    StaticJsonDocument<160> doc;
    doc["ok"] = false;
    doc["error"] = "Not found";
    sendJsonResponse(404, doc);
  });
  server.begin();
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
#ifdef DEVICE_HOSTNAME
  WiFi.setHostname(DEVICE_HOSTNAME);
#endif
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print('.');
    if (millis() - startedAt > 30000) {
      Serial.println("\nWi-Fi timeout. Rebooting...");
      ESP.restart();
    }
  }

  Serial.println();
  Serial.print("Connected. IP: ");
  Serial.println(WiFi.localIP());
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  strip.begin();
  strip.clear();
  strip.show();

  connectWiFi();
  setupServer();

  // Startup indicator: soft green
  applyColor(34, 197, 94, currentBrightness);
}

void loop() {
  server.handleClient();
}
