#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include "timing.h"

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
// Host resends every 10 seconds; turn off after 15 seconds without updates.
constexpr uint32_t kHostTimeoutMs = 15000;
uint32_t lastHostUpdateAt = 0;
bool hostTimedOut = false;
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

  }

  strip.setBrightness(brightness);
  strip.clear();
  bool hasPrimaryColor = false;
  uint8_t parsedSegmentCount = 0;
  for (JsonObjectConst segment : segments) {
    const int start = segment["start"];
    const int end = segment["end"];
    const int r = segment["r"];
    const int g = segment["g"];
    const int b = segment["b"];
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
  JsonDocument doc;
  doc["ok"] = true;
  doc["ip"] = WiFi.localIP().toString();
  doc["theme"] = currentTheme;
  doc["r"] = currentR;
  doc["g"] = currentG;
  doc["b"] = currentB;
  doc["brightness"] = currentBrightness;
  doc["mode"] = segmentedMode ? "segmented" : "solid";
  doc["segment_count"] = segmentCount;
  doc["host_timed_out"] = hostTimedOut;

  sendJsonResponse(200, doc);
}

void handleSetColor() {
  JsonDocument doc;

  if (!server.hasArg("plain")) {
    doc["ok"] = false;
    doc["error"] = "Expected JSON body";
    sendJsonResponse(400, doc);
    return;
  }

  JsonDocument input;
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

  JsonArrayConst segments = input["segments"].as<JsonArrayConst>();
  if (!input["segments"].isNull() && segments.isNull()) {
    doc["ok"] = false;
    doc["error"] = "'segments' must be an array";
    sendJsonResponse(400, doc);
    return;
  }
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

  currentTheme = theme;
  lastHostUpdateAt = millis();
  hostTimedOut = false;

  doc["ok"] = true;
  doc["theme"] = currentTheme;
  doc["r"] = currentR;
  doc["g"] = currentG;
  doc["b"] = currentB;
  doc["brightness"] = currentBrightness;
  doc["mode"] = segmentedMode ? "segmented" : "solid";
  doc["segment_count"] = segmentCount;
  doc["host_timed_out"] = hostTimedOut;
  sendJsonResponse(200, doc);
}

void setupServer() {
  server.on("/health", HTTP_GET, handleHealth);
  server.on("/color", HTTP_POST, handleSetColor);
  server.onNotFound([]() {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = "Not found";
    sendJsonResponse(404, doc);
  });
  server.begin();
}

// Inclusive ranges matching host/config.example.json. Adjust for rewired logos.
bool isArchLed(uint16_t led) {
  return led <= 22 || led == 39 || led >= 73;
}

void showStartup(bool archOn) {
  strip.setBrightness(LED_BRIGHTNESS);
  for (uint16_t led = 0; led < LED_COUNT; ++led) {
    strip.setPixelColor(led, isArchLed(led)
        ? strip.Color(0, 0, archOn ? 255 : 0)
        : strip.Color(0, 255, 0));
  }
  strip.show();
  currentTheme = "startup";
  segmentedMode = true;
  segmentCount = 5;
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
#ifdef DEVICE_HOSTNAME
  WiFi.setHostname(DEVICE_HOSTNAME);
#endif
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  strip.begin();
  strip.clear();
  strip.show();

  showStartup(true);
  connectWiFi();
  setupServer();
}

void loop() {
  static bool wasConnected = false;
  static uint32_t lastBlink = 0;
  static uint32_t lastRetry = 0;
  static bool archOn = true;
  const uint32_t now = millis();
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (!connected) {
    if (!hostTimedOut && timerExpired(now, lastBlink, 500)) {
      lastBlink = now;
      archOn = !archOn;
      showStartup(archOn);
    }
    if (timerExpired(now, lastRetry, 30000)) {
      lastRetry = now;
      WiFi.reconnect();
    }
  } else {
    if (!wasConnected) {
      if (!hostTimedOut) showStartup(true);
      Serial.print("Connected. IP: ");
      Serial.println(WiFi.localIP());
    }
    server.handleClient();
  }
  // Check after handling requests so a new heartbeat immediately restores light.
  if (!hostTimedOut && timerExpired(millis(), lastHostUpdateAt, kHostTimeoutMs)) {
    applyColor(0, 0, 0, currentBrightness);
    currentTheme = "host-offline";
    hostTimedOut = true;
  }
  wasConnected = connected;
  delay(1);
}
