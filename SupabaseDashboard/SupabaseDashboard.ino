// XIAO ESP32-S3 Sense dashboard: local web + Supabase cloud sync + camera
// - LED (GPIO21, LOW = ON), touch sensor D0 (GPIO1, TOUCH1)
// - port 80: local dashboard  /, /api/status, /api/led?state=on|off|toggle, /api/camera?size=N
// - port 81: camera  /stream (MJPEG, up to 3 viewers), /capture (JPEG)  -> camera_stream.h
// - cloud: RPC public.xiao_report every REPORT_MS; response = desired LED state
//   LED changed on the local dashboard is pushed first (ledPending), then read back
// build: arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi SupabaseDashboard
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "secrets.h"
#include "camera_stream.h"
#include "dashboard_html.h"

#define LED_PIN   21
#define TOUCH_PIN 1

const uint32_t REPORT_MS = 200;
const uint32_t CLOUD_STALE_MS = 5000;

WebServer server(80);
WiFiClientSecure tls;
HTTPClient http;

bool ledOn = false;
bool ledPending = false;  // local LED change not yet pushed to the cloud
bool touched = false;
uint32_t baseline;
uint32_t touchValue;
uint32_t lastCloudOk = 0;

void setLed(bool on) {
  ledOn = on;
  digitalWrite(LED_PIN, on ? LOW : HIGH);
}

bool cloudOk() {
  return lastCloudOk && millis() - lastCloudOk < CLOUD_STALE_MS;
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
         ",\"camera\":" + (cameraOk ? "true" : "false") +
         ",\"viewers\":" + cameraViewers() +
         ",\"cloud\":" + (cloudOk() ? "true" : "false") +
         ",\"rssi\":" + WiFi.RSSI() +
         ",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
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
  ledPending = true;
  server.send(200, "application/json", statusJson());
}

void handleCamera() {
  if (!setCameraFrameSize(server.arg("size").toInt())) {
    server.send(400, "application/json", "{\"error\":\"camera off or size out of range (5..11)\"}");
    return;
  }
  server.send(200, "application/json", statusJson());
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);  // lower latency for the camera stream
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// POST /rest/v1/rpc/xiao_report -> "true" / "false"
bool report() {
  bool pushLed = ledPending;
  String url = String(SUPABASE_URL) + "/rest/v1/rpc/xiao_report";
  String body = String("{\"p_device\":\"") + DEVICE_ID +
                "\",\"p_secret\":\"" + DEVICE_SECRET +
                "\",\"p_touched\":" + (touched ? "true" : "false") +
                ",\"p_value\":" + touchValue +
                ",\"p_baseline\":" + baseline +
                ",\"p_ip\":\"" + WiFi.localIP().toString() + "\"";
  if (pushLed) {
    body += String(",\"p_led\":") + (ledOn ? "true" : "false");
  }
  body += "}";

  if (!http.begin(tls, url)) {
    Serial.println("http.begin failed");
    return false;
  }
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Content-Type", "application/json");

  int code = http.POST(body);
  String resp = http.getString();
  http.end();  // keeps TLS connection alive (setReuse)

  if (code != 200) {
    Serial.printf("report failed: %d %s\n", code, resp.c_str());
    return false;
  }
  lastCloudOk = millis();
  if (pushLed) ledPending = false;
  resp.trim();
  setLed(resp == "true");
  return true;
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  setLed(false);

  calibrateTouch();
  cameraOk = initCamera();
  connectWiFi();

  if (MDNS.begin("xiao")) {
    MDNS.addService("http", "tcp", 80);
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/led", handleLed);
  server.on("/api/camera", handleCamera);
  server.begin();

  if (cameraOk) {
    startCameraServer();
  }

  // no CA pinning: Supabase certs rotate; traffic is still encrypted
  tls.setInsecure();
  http.setReuse(true);
  http.setTimeout(5000);
}

void loop() {
  static uint32_t lastTouch = 0;
  static uint32_t lastReport = 0;
  static uint32_t lastPrint = 0;
  static bool lastTouched = false;

  server.handleClient();

  if (millis() - lastTouch >= 50) {
    lastTouch = millis();
    updateTouch();
  }

  // report periodically, or right away when touch or local LED changes
  if (WiFi.status() == WL_CONNECTED &&
      (millis() - lastReport >= REPORT_MS || touched != lastTouched || ledPending)) {
    lastReport = millis();
    lastTouched = touched;
    report();
  }

  if (millis() - lastPrint >= 5000) {
    lastPrint = millis();
    Serial.printf("IP=%s touch=%d(%lu/%lu) led=%d cloud=%d cam=%d viewers=%d\n",
                  WiFi.localIP().toString().c_str(), touched, touchValue, baseline,
                  ledOn, cloudOk(), cameraOk, cameraViewers());
  }
}
