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

constexpr size_t kMaxSegments = 16;

uint8_t currentR = 0;
uint8_t currentG = 0;
uint8_t currentB = 0;
uint8_t currentBrightness = LED_BRIGHTNESS;
String currentTheme = "unknown";
bool segmentedMode = false;
uint8_t segmentCount = 0;

void applyColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
  currentR = r;
  currentG = g;
  currentB = b;
  currentBrightness = brightness;
  segmentedMode = false;
  segmentCount = 0;

  strip.setBrightness(currentBrightness);
  strip.fill(strip.Color(currentR, currentG, currentB));
  strip.show();
}

bool applySegments(JsonArrayConst segments, uint8_t brightness, String& errorMessage) {
  if (segments.isNull() || segments.size() == 0) {
    errorMessage = "'segments' must be a non-empty array";
    return false;
  }
  if (segments.size() > kMaxSegments) {
    errorMessage = "Too many segments";
    return false;
  }

  strip.setBrightness(brightness);
  strip.clear();

  uint8_t parsedSegmentCount = 0;
  bool hasPrimaryColor = false;

  for (JsonVariantConst segmentVariant : segments) {
    JsonObjectConst segment = segmentVariant.as<JsonObjectConst>();
    if (segment.isNull()) {
      errorMessage = "Each segment must be an object";
      return false;
    }

    int start = segment["start"] | -1;
    int end = segment["end"] | -1;
    int r = segment["r"] | -1;
    int g = segment["g"] | -1;
    int b = segment["b"] | -1;

    if (start < 0 || end < 0 || start > end || end >= LED_COUNT) {
      errorMessage = "Invalid segment range";
      return false;
    }
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
      errorMessage = "Segment RGB values must be 0-255";
      return false;
    }

    if (!hasPrimaryColor) {
      currentR = static_cast<uint8_t>(r);
      currentG = static_cast<uint8_t>(g);
      currentB = static_cast<uint8_t>(b);
      hasPrimaryColor = true;
    }

    for (int led = start; led <= end; ++led) {
      strip.setPixelColor(led, strip.Color(r, g, b));
    }

    ++parsedSegmentCount;
  }

  strip.show();

  currentBrightness = brightness;
  segmentedMode = true;
  segmentCount = parsedSegmentCount;
  return true;
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
  doc["mode"] = segmentedMode ? "segmented" : "solid";
  doc["segment_count"] = segmentCount;

  sendJsonResponse(200, doc);
}

void handleSetColor() {
  StaticJsonDocument<384> doc;

  if (!server.hasArg("plain")) {
    doc["ok"] = false;
    doc["error"] = "Expected JSON body";
    sendJsonResponse(400, doc);
    return;
  }

  StaticJsonDocument<1536> input;
  DeserializationError error = deserializeJson(input, server.arg("plain"));
  if (error) {
    doc["ok"] = false;
    doc["error"] = "Invalid JSON";
    sendJsonResponse(400, doc);
    return;
  }

  int brightness = input["brightness"] | currentBrightness;
  const char* theme = input["theme"] | "manual";
  brightness = constrain(brightness, 0, 255);

  currentTheme = theme;

  JsonArrayConst segments = input["segments"].as<JsonArrayConst>();
  if (!segments.isNull()) {
    String segmentError;
    if (!applySegments(segments, static_cast<uint8_t>(brightness), segmentError)) {
      doc["ok"] = false;
      doc["error"] = segmentError;
      sendJsonResponse(400, doc);
      return;
    }
  } else {
    int r = input["r"] | currentR;
    int g = input["g"] | currentG;
    int b = input["b"] | currentB;

    r = constrain(r, 0, 255);
    g = constrain(g, 0, 255);
    b = constrain(b, 0, 255);

    applyColor(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), static_cast<uint8_t>(brightness));
  }

  doc["ok"] = true;
  doc["theme"] = currentTheme;
  doc["r"] = currentR;
  doc["g"] = currentG;
  doc["b"] = currentB;
  doc["brightness"] = currentBrightness;
  doc["mode"] = segmentedMode ? "segmented" : "solid";
  doc["segment_count"] = segmentCount;
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
