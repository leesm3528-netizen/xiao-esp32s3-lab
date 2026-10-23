// XIAO ESP32-S3 AP mode dashboard (no internet needed)
// - board broadcasts its own Wi-Fi (AP_SSID in secrets.h); phone joins it
// - web server at http://192.168.4.1/ : touch value, LED on/off
// - LED (GPIO21, LOW = ON), touch sensor D0 (GPIO1, TOUCH1)
// - API: /api/status, /api/led?state=on|off|toggle
// build: arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3 APDashboard
#include <WiFi.h>
#include <WebServer.h>
#include "secrets.h"
#include "dashboard_html.h"

#define LED_PIN   21
#define TOUCH_PIN 1

const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_MASK(255, 255, 255, 0);

WebServer server(80);

bool ledOn = false;
bool touched = false;
uint32_t baseline;
uint32_t touchValue;

void setLed(bool on) {
  ledOn = on;
  digitalWrite(LED_PIN, on ? LOW : HIGH);
}

void calibrateTouch() {
  // discard unstable readings right after boot
  for (int i = 0; i < 10; i++) {
    touchRead(TOUCH_PIN);
    delay(20);
  }
  // calibrate baseline (don't touch during boot)
  uint64_t sum = 0;
  for (int i = 0; i < 20; i++) {
    sum += touchRead(TOUCH_PIN);
    delay(10);
  }
  baseline = sum / 20;
}

void updateTouch() {
  touchValue = touchRead(TOUCH_PIN);
  touched = touchValue > baseline * 1.3;  // 30% above baseline
  // track baseline while not touched
  if (!touched) {
    baseline = (baseline * 15 + touchValue) / 16;
  }
}

String statusJson() {
  return String("{\"led\":") + (ledOn ? "true" : "false") +
         ",\"touched\":" + (touched ? "true" : "false") +
         ",\"value\":" + touchValue +
         ",\"baseline\":" + baseline +
         ",\"clients\":" + WiFi.softAPgetStationNum() + "}";
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", DASHBOARD_HTML);
}

void handleStatus() {
  server.send(200, "application/json", statusJson());
}

void handleLed() {
  String state = server.arg("state");
  if (state == "on") {
    setLed(true);
  } else if (state == "off") {
    setLed(false);
  } else if (state == "toggle") {
    setLed(!ledOn);
  } else {
    server.send(400, "application/json", "{\"error\":\"state must be on, off or toggle\"}");
    return;
  }
  server.send(200, "application/json", statusJson());
}

void handleNotFound() {
  // any other path (e.g. phone connectivity checks) -> dashboard
  server.sendHeader("Location", "http://192.168.4.1/");
  server.send(302, "text/plain", "");
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  setLed(false);

  calibrateTouch();

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);
  if (!WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 4)) {  // channel 1, visible, max 4 devices
    Serial.println("softAP start failed");
  }
  Serial.printf("AP \"%s\" started, open http://%s/\n", AP_SSID, WiFi.softAPIP().toString().c_str());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/led", handleLed);
  server.onNotFound(handleNotFound);
  server.begin();
}

void loop() {
  static uint32_t lastTouch = 0;
  static uint32_t lastPrint = 0;

  server.handleClient();

  if (millis() - lastTouch >= 50) {
    lastTouch = millis();
    updateTouch();
  }

  if (millis() - lastPrint >= 5000) {
    lastPrint = millis();
    Serial.printf("AP=%s clients=%d touch=%d(%lu/%lu) led=%d\n",
                  WiFi.softAPIP().toString().c_str(), WiFi.softAPgetStationNum(),
                  touched, touchValue, baseline, ledOn);
  }
}
