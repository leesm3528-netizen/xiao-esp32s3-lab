// Camera server on port 81: /stream (MJPEG) and /capture (JPEG)
// - WiFiServer, not esp_http_server: httpd's 1024-byte header limit is baked into the core and
//   breaks phones behind Tailscale Serve (431). Only the request line is read; headers are skipped.
// - one FreeRTOS task per client, so viewers and /capture never block each other
#pragma once
#include <WiFi.h>
#include "esp_camera.h"

#define CAM_PORT      81
#define MAX_CLIENTS   6
#define MAX_STREAMS   3
#define FB_COUNT      3
#define HEADER_SKIP_TIMEOUT_MS 2000

// XIAO ESP32-S3 Sense camera pins (Xiao.md Camera table)
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  10
#define SIOD_GPIO_NUM  40
#define SIOC_GPIO_NUM  39
#define Y9_GPIO_NUM    48
#define Y8_GPIO_NUM    11
#define Y7_GPIO_NUM    12
#define Y6_GPIO_NUM    14
#define Y5_GPIO_NUM    16
#define Y4_GPIO_NUM    18
#define Y3_GPIO_NUM    17
#define Y2_GPIO_NUM    15
#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM  47
#define PCLK_GPIO_NUM  13

#define PART_BOUNDARY "xiaoframe"

static WiFiServer camServer(CAM_PORT);
static volatile int camClients = 0;
static volatile int camStreams = 0;
static portMUX_TYPE camMux = portMUX_INITIALIZER_UNLOCKED;
bool cameraOk = false;

bool initCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count = FB_COUNT;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (!psramFound()) {
    Serial.println("PSRAM not found: build with PSRAM=opi");
    return false;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }
  return true;
}

// FRAMESIZE_QVGA..FRAMESIZE_HD
bool setCameraFrameSize(int size) {
  if (!cameraOk || size < FRAMESIZE_QVGA || size > FRAMESIZE_HD) return false;
  sensor_t *s = esp_camera_sensor_get();
  return s->set_framesize(s, (framesize_t)size) == 0;
}

// read "GET /path?query HTTP/1.1", return path without query; skip all headers whatever their length
static bool readRequestPath(WiFiClient &client, String &path) {
  uint32_t start = millis();
  String line;
  while (client.connected() && millis() - start < HEADER_SKIP_TIMEOUT_MS) {
    if (!client.available()) {
      delay(1);
      continue;
    }
    char c = client.read();
    if (c == '\n') break;
    if (c != '\r' && line.length() < 256) line += c;
  }
  // drain headers until an empty line, without storing them (request line's \n counts as the first)
  int newlines = 1;
  while (client.connected() && millis() - start < HEADER_SKIP_TIMEOUT_MS && newlines < 2) {
    if (!client.available()) {
      delay(1);
      continue;
    }
    char c = client.read();
    if (c == '\n') newlines++;
    else if (c != '\r') newlines = 0;
  }

  int sp1 = line.indexOf(' ');
  int sp2 = line.indexOf(' ', sp1 + 1);
  if (sp1 < 0) return false;
  path = line.substring(sp1 + 1, sp2 < 0 ? line.length() : sp2);
  int q = path.indexOf('?');
  if (q >= 0) path = path.substring(0, q);
  return true;
}

static void sendSimple(WiFiClient &client, const char *status, const char *body) {
  client.printf("HTTP/1.1 %s\r\nContent-Type: text/plain\r\nAccess-Control-Allow-Origin: *\r\n"
                "Connection: close\r\nContent-Length: %u\r\n\r\n%s",
                status, (unsigned)strlen(body), body);
}

static void handleCapture(WiFiClient &client) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    sendSimple(client, "500 Internal Server Error", "capture failed");
    return;
  }
  client.printf("HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\n"
                "Content-Disposition: inline; filename=xiao.jpg\r\nCache-Control: no-cache\r\n"
                "Access-Control-Allow-Origin: *\r\nConnection: close\r\nContent-Length: %u\r\n\r\n",
                (unsigned)fb->len);
  client.write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

static void handleStream(WiFiClient &client) {
  bool admitted = false;
  portENTER_CRITICAL(&camMux);
  if (camStreams < MAX_STREAMS) {
    camStreams++;
    admitted = true;
  }
  portEXIT_CRITICAL(&camMux);
  if (!admitted) {
    sendSimple(client, "503 Service Unavailable", "too many viewers");
    return;
  }

  client.print("HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace;boundary=" PART_BOUNDARY "\r\n"
               "Cache-Control: no-cache\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n");

  char part[96];
  while (client.connected()) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      delay(10);
      continue;
    }
    int n = snprintf(part, sizeof(part),
                     "--" PART_BOUNDARY "\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                     (unsigned)fb->len);
    bool ok = client.write((const uint8_t *)part, n) == (size_t)n &&
              client.write(fb->buf, fb->len) == fb->len &&
              client.write((const uint8_t *)"\r\n", 2) == 2;
    esp_camera_fb_return(fb);
    if (!ok) break;
    delay(1);  // let other tasks run
  }

  portENTER_CRITICAL(&camMux);
  camStreams--;
  portEXIT_CRITICAL(&camMux);
}

static void camClientTask(void *arg) {
  WiFiClient *client = (WiFiClient *)arg;
  client->setNoDelay(true);

  String path;
  if (readRequestPath(*client, path)) {
    if (path == "/stream") handleStream(*client);
    else if (path == "/capture") handleCapture(*client);
    else sendSimple(*client, "404 Not Found", "use /stream or /capture");
  }

  client->stop();
  delete client;
  portENTER_CRITICAL(&camMux);
  camClients--;
  portEXIT_CRITICAL(&camMux);
  vTaskDelete(NULL);
}

static void camAcceptTask(void *) {
  for (;;) {
    WiFiClient incoming = camServer.accept();
    if (!incoming) {
      delay(5);
      continue;
    }
    bool admitted = false;
    portENTER_CRITICAL(&camMux);
    if (camClients < MAX_CLIENTS) {
      camClients++;
      admitted = true;
    }
    portEXIT_CRITICAL(&camMux);
    if (!admitted) {
      sendSimple(incoming, "503 Service Unavailable", "too many connections");
      incoming.stop();
      continue;
    }
    WiFiClient *client = new WiFiClient(incoming);
    if (xTaskCreatePinnedToCore(camClientTask, "camClient", 8192, client, 2, NULL, 0) != pdPASS) {
      client->stop();
      delete client;
      portENTER_CRITICAL(&camMux);
      camClients--;
      portEXIT_CRITICAL(&camMux);
    }
  }
}

void startCameraServer() {
  camServer.begin();
  camServer.setNoDelay(true);
  xTaskCreatePinnedToCore(camAcceptTask, "camAccept", 4096, NULL, 2, NULL, 0);
}

int cameraViewers() {
  return camStreams;
}
