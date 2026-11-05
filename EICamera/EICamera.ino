// XIAO ESP32-S3 Sense camera for Edge Impulse data collection
// - port 81: /stream (MJPEG, up to 3 viewers), /capture (JPEG)  -> camera_stream.h
// - port 80: /api/status, /api/camera?size=N&quality=Q  (JSON, CORS open)
// - images are saved by the PC collector (../EICollector), not on the board
// build: arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi EICamera
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "secrets.h"
#include "camera_stream.h"

#define LED_PIN 21  // active-low

const int START_FRAMESIZE = FRAMESIZE_240X240;
const int START_QUALITY = 12;

WebServer server(80);

String statusJson() {
  char buf[256];
  snprintf(buf, sizeof(buf),
           "{\"ip\":\"%s\",\"rssi\":%d,\"camera\":%s,\"framesize\":%d,\"quality\":%d,"
           "\"viewers\":%d,\"heap\":%u,\"psram\":%u,\"uptime\":%lu}",
           WiFi.localIP().toString().c_str(), WiFi.RSSI(), cameraOk ? "true" : "false",
           cameraFrameSize(), cameraQuality(), cameraViewers(),
           (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram(), millis() / 1000);
  return String(buf);
}

void sendJson(int code, const String &body) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache");
  server.send(code, "application/json", body);
}

void handleRoot() {
  server.send(200, "text/plain",
              "XIAO EICamera\n"
              "  :81/stream   MJPEG\n"
              "  :81/capture  JPEG\n"
              "  /api/status\n"
              "  /api/camera?size=0..13&quality=10..63\n"
              "Dashboard: run EICollector/server.py on the PC\n");
}

void handleStatus() {
  sendJson(200, statusJson());
}

void handleCamera() {
  if (server.hasArg("size") && !setCameraFrameSize(server.arg("size").toInt())) {
    sendJson(400, "{\"error\":\"camera off or size out of range (0..13)\"}");
    return;
  }
  if (server.hasArg("quality") && !setCameraQuality(server.arg("quality").toInt())) {
    sendJson(400, "{\"error\":\"camera off or quality out of range (10..63)\"}");
    return;
  }
  sendJson(200, statusJson());
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);  // lower latency for stream and capture
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

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);  // never block loop() when nobody reads the COM port
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // off

  cameraOk = initCamera();
  if (cameraOk) {
    setCameraFrameSize(START_FRAMESIZE);
    setCameraQuality(START_QUALITY);
  }
  connectWiFi();

  if (MDNS.begin("xiao")) {
    MDNS.addService("http", "tcp", 80);
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/camera", handleCamera);
  server.begin();

  if (cameraOk) {
    startCameraServer();
    digitalWrite(LED_PIN, LOW);  // LED on = camera ready
  }
}

void loop() {
  static uint32_t lastPrint = 0;
  server.handleClient();

  if (millis() - lastPrint >= 5000) {
    lastPrint = millis();
    Serial.printf("IP %s | cam %s | size %d q %d | viewers %d | rssi %d\n",
                  WiFi.localIP().toString().c_str(), cameraOk ? "ok" : "NONE",
                  cameraFrameSize(), cameraQuality(), cameraViewers(), WiFi.RSSI());
  }
  delay(2);
}
