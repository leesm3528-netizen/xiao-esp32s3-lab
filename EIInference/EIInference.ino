// XIAO ESP32-S3 Sense: Edge Impulse image classification (Mint / WaterBottle)
// - camera stays JPEG 240x240 (same as the training photos), so the stream keeps working
// - inference runs in its own task: JPEG -> RGB888 -> 96x96 -> run_classifier()
// - port 80: /  dashboard (live view, model input, result, history, board status)
//            /api/result  /api/history  /api/input.bmp (the 96x96 image the model saw)
//            /api/config?paused=0|1&threshold=0.5..0.99&label=<name>
// - port 81: /stream, /capture  (camera_stream.h)
// - serial: one line per inference; LED (GPIO21, active-low) on while ledLabel wins >= threshold
// library: EIModel/ei-mint-waterbottle-arduino-v1.zip (see EIInference/README.md)
// build: arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi --jobs 2 EIInference
#include <leesm3528-netizen-project-1_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "img_converters.h"
#include "secrets.h"
#include "camera_stream.h"
#include "page_html.h"

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_CAMERA
#error "This library is not an image model"
#endif

#define LED_PIN 21  // active-low
const uint32_t INFER_INTERVAL_MS = 200;
const int MAX_W = 640, MAX_H = 480;  // camera buffers are sized for VGA
const int IN_W = EI_CLASSIFIER_INPUT_WIDTH, IN_H = EI_CLASSIFIER_INPUT_HEIGHT;
const int NLABELS = EI_CLASSIFIER_LABEL_COUNT;
const int HISTORY = 30;

// settings changeable from the dashboard
static volatile int ledLabel = NLABELS - 1;  // WaterBottle
static volatile float threshold = 0.80f;
static volatile bool paused = false;

static uint8_t *rgbBuf = nullptr;    // decoded frame (BGR888), resized in place to 96x96
static uint8_t *inputCopy = nullptr; // last model input for /api/input.bmp
static SemaphoreHandle_t inputLock;

struct Result {
  float scores[NLABELS];
  int top;
  int dspMs, nnMs, totalMs;
  int srcW, srcH, srcBytes;  // the camera frame that was classified
  uint32_t count;
  uint32_t at;
  bool ok;
};
static Result lastResult = {};
static Result history[HISTORY];
static int histHead = 0, histLen = 0;
static uint32_t labelCount[NLABELS] = {};
static float fps = 0;
static portMUX_TYPE resultMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool ledOn = false;

WebServer server(80);

// EI asks for pixels as packed 0xRRGGBB floats; fmt2rgb888 writes B,G,R
static int getData(size_t offset, size_t length, float *out) {
  size_t px = offset * 3;
  for (size_t i = 0; i < length; i++, px += 3) {
    out[i] = (rgbBuf[px + 2] << 16) + (rgbBuf[px + 1] << 8) + rgbBuf[px];
  }
  return 0;
}

static bool inferOnce(Result &r) {
  uint32_t t0 = millis();
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return false;
  r.srcW = fb->width;
  r.srcH = fb->height;
  r.srcBytes = fb->len;
  bool ok = r.srcW <= MAX_W && r.srcH <= MAX_H &&
            fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgbBuf);
  esp_camera_fb_return(fb);
  if (!ok) return false;

  // center-crop to square and resize (training used "fit shortest side")
  ei::image::processing::crop_and_interpolate_rgb888(rgbBuf, r.srcW, r.srcH, rgbBuf, IN_W, IN_H);
  if (xSemaphoreTake(inputLock, pdMS_TO_TICKS(50)) == pdTRUE) {
    memcpy(inputCopy, rgbBuf, IN_W * IN_H * 3);
    xSemaphoreGive(inputLock);
  }

  ei::signal_t signal;
  signal.total_length = IN_W * IN_H;
  signal.get_data = &getData;
  ei_impulse_result_t res = {};
  if (run_classifier(&signal, &res, false) != EI_IMPULSE_OK) return false;

  r.top = 0;
  for (int i = 0; i < NLABELS; i++) {
    r.scores[i] = res.classification[i].value;
    if (r.scores[i] > r.scores[r.top]) r.top = i;
  }
  r.dspMs = res.timing.dsp;
  r.nnMs = res.timing.classification;
  r.totalMs = millis() - t0;
  r.at = millis();
  r.ok = true;
  return true;
}

static void inferTask(void *) {
  uint32_t count = 0, fails = 0, lastAt = 0;
  for (;;) {
    uint32_t start = millis();
    if (paused) {
      delay(100);
      continue;
    }
    Result r = {};
    if (inferOnce(r)) {
      r.count = ++count;
      float dt = lastAt ? (r.at - lastAt) / 1000.0f : 0;
      lastAt = r.at;
      portENTER_CRITICAL(&resultMux);
      lastResult = r;
      history[histHead] = r;
      histHead = (histHead + 1) % HISTORY;
      if (histLen < HISTORY) histLen++;
      labelCount[r.top]++;
      if (dt > 0) fps = fps == 0 ? 1 / dt : fps * 0.8f + 0.2f / dt;
      portEXIT_CRITICAL(&resultMux);

      ledOn = r.top == ledLabel && r.scores[r.top] >= threshold;
      digitalWrite(LED_PIN, ledOn ? LOW : HIGH);

      Serial.printf("#%lu %s %.2f |", (unsigned long)count,
                    ei_classifier_inferencing_categories[r.top], r.scores[r.top]);
      for (int i = 0; i < NLABELS; i++) {
        Serial.printf(" %s %.2f", ei_classifier_inferencing_categories[i], r.scores[i]);
      }
      Serial.printf(" | dsp %d nn %d total %d ms\n", r.dspMs, r.nnMs, r.totalMs);
    } else if (++fails % 20 == 1) {
      Serial.printf("inference failed (%lu)\n", (unsigned long)fails);
    }
    uint32_t spent = millis() - start;
    delay(spent < INFER_INTERVAL_MS ? INFER_INTERVAL_MS - spent : 1);
  }
}

// ---------- HTTP ----------

static void appendScores(String &s, const Result &r) {
  s += "{";
  for (int i = 0; i < NLABELS; i++) {
    if (i) s += ",";
    s += "\"";
    s += ei_classifier_inferencing_categories[i];
    s += "\":";
    s += String(r.scores[i], 3);
  }
  s += "}";
}

static void sendJson(const String &body, int code = 200) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache");
  server.send(code, "application/json", body);
}

void handleResult() {
  Result r;
  uint32_t counts[NLABELS];
  float f;
  portENTER_CRITICAL(&resultMux);
  r = lastResult;
  memcpy(counts, labelCount, sizeof(counts));
  f = fps;
  portEXIT_CRITICAL(&resultMux);

  String s;
  s.reserve(900);
  s += "{\"ok\":";
  s += r.ok ? "true" : "false";
  s += ",\"label\":\"";
  s += r.ok ? ei_classifier_inferencing_categories[r.top] : "";
  s += "\",\"score\":";
  s += r.ok ? String(r.scores[r.top], 3) : "0";
  s += ",\"scores\":";
  appendScores(s, r);
  s += ",\"counts\":{";
  for (int i = 0; i < NLABELS; i++) {
    if (i) s += ",";
    s += "\"";
    s += ei_classifier_inferencing_categories[i];
    s += "\":";
    s += counts[i];
  }
  s += "},\"labels\":[";
  for (int i = 0; i < NLABELS; i++) {
    if (i) s += ",";
    s += "\"";
    s += ei_classifier_inferencing_categories[i];
    s += "\"";
  }
  char tail[512];
  snprintf(tail, sizeof(tail),
           "],\"dsp_ms\":%d,\"nn_ms\":%d,\"total_ms\":%d,\"fps\":%.2f,\"count\":%lu,\"age_ms\":%lu,"
           "\"src\":{\"w\":%d,\"h\":%d,\"bytes\":%d},\"input\":{\"w\":%d,\"h\":%d},"
           "\"camera\":{\"ok\":%s,\"framesize\":%d,\"quality\":%d,\"viewers\":%d},"
           "\"led\":%s,\"led_label\":\"%s\",\"threshold\":%.2f,\"paused\":%s,"
           "\"board\":{\"ip\":\"%s\",\"rssi\":%d,\"heap\":%u,\"psram\":%u,\"uptime\":%lu}}",
           r.dspMs, r.nnMs, r.totalMs, f, (unsigned long)r.count,
           (unsigned long)(r.ok ? millis() - r.at : 0), r.srcW, r.srcH, r.srcBytes, IN_W, IN_H,
           cameraOk ? "true" : "false", cameraFrameSize(), cameraQuality(), cameraViewers(),
           ledOn ? "true" : "false", ei_classifier_inferencing_categories[ledLabel], threshold,
           paused ? "true" : "false", WiFi.localIP().toString().c_str(), WiFi.RSSI(),
           (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram(), millis() / 1000);
  s += tail;
  sendJson(s);
}

void handleHistory() {
  Result items[HISTORY];
  int n, head;
  portENTER_CRITICAL(&resultMux);
  n = histLen;
  head = histHead;
  memcpy(items, history, sizeof(items));
  portEXIT_CRITICAL(&resultMux);

  uint32_t now = millis();
  String s;
  s.reserve(n * 90 + 20);
  s += "{\"items\":[";
  for (int k = 0; k < n; k++) {  // newest first
    const Result &r = items[(head - 1 - k + HISTORY) % HISTORY];
    if (k) s += ",";
    s += "{\"n\":";
    s += r.count;
    s += ",\"label\":\"";
    s += ei_classifier_inferencing_categories[r.top];
    s += "\",\"score\":";
    s += String(r.scores[r.top], 3);
    s += ",\"ago_ms\":";
    s += now - r.at;
    s += "}";
  }
  s += "]}";
  sendJson(s);
}

// 96x96 model input as a 24-bit BMP (BMP stores B,G,R, same as the buffer; negative height = top-down)
void handleInputBmp() {
  const uint32_t rowBytes = IN_W * 3;  // 288, already a multiple of 4
  const uint32_t dataBytes = rowBytes * IN_H;
  uint8_t hdr[54] = {'B', 'M'};
  auto put32 = [&](int at, uint32_t v) { memcpy(hdr + at, &v, 4); };
  put32(2, 54 + dataBytes);
  put32(10, 54);
  put32(14, 40);
  put32(18, IN_W);
  put32(22, (uint32_t)(-IN_H));
  hdr[26] = 1;
  hdr[28] = 24;
  put32(34, dataBytes);

  uint8_t *body = (uint8_t *)ps_malloc(54 + dataBytes);
  if (!body) {
    sendJson("{\"error\":\"no memory\"}", 500);
    return;
  }
  memcpy(body, hdr, 54);
  if (xSemaphoreTake(inputLock, pdMS_TO_TICKS(200)) == pdTRUE) {
    memcpy(body + 54, inputCopy, dataBytes);
    xSemaphoreGive(inputLock);
  } else {
    memset(body + 54, 0, dataBytes);
  }
  server.sendHeader("Cache-Control", "no-cache");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.setContentLength(54 + dataBytes);
  server.send(200, "image/bmp", "");
  server.sendContent((const char *)body, 54 + dataBytes);
  free(body);
}

void handleConfig() {
  if (server.hasArg("paused")) paused = server.arg("paused") == "1";
  if (server.hasArg("threshold")) {
    float t = server.arg("threshold").toFloat();
    if (t < 0.5f || t > 0.99f) {
      sendJson("{\"error\":\"threshold 0.5..0.99\"}", 400);
      return;
    }
    threshold = t;
  }
  if (server.hasArg("label")) {
    int found = -1;
    for (int i = 0; i < NLABELS; i++) {
      if (server.arg("label") == ei_classifier_inferencing_categories[i]) found = i;
    }
    if (found < 0) {
      sendJson("{\"error\":\"unknown label\"}", 400);
      return;
    }
    ledLabel = found;
  }
  if (paused) {
    ledOn = false;
    digitalWrite(LED_PIN, HIGH);
  }
  handleResult();
}

void handleReset() {
  portENTER_CRITICAL(&resultMux);
  histLen = 0;
  histHead = 0;
  memset(labelCount, 0, sizeof(labelCount));
  portEXIT_CRITICAL(&resultMux);
  handleResult();
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
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
  Serial.setTxTimeoutMs(0);  // never block when nobody reads the COM port
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // off

  inputLock = xSemaphoreCreateMutex();
  rgbBuf = (uint8_t *)ps_malloc(MAX_W * MAX_H * 3);
  inputCopy = (uint8_t *)ps_calloc(IN_W * IN_H * 3, 1);
  cameraOk = rgbBuf && inputCopy && initCamera();
  if (cameraOk) {
    setCameraFrameSize(FRAMESIZE_240X240);
    setCameraQuality(12);
  } else {
    Serial.println("camera or PSRAM buffer failed: no inference");
  }
  connectWiFi();
  if (MDNS.begin("xiao")) MDNS.addService("http", "tcp", 80);

  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", PAGE_HTML); });
  server.on("/api/result", HTTP_GET, handleResult);
  server.on("/api/history", HTTP_GET, handleHistory);
  server.on("/api/input.bmp", HTTP_GET, handleInputBmp);
  server.on("/api/config", handleConfig);
  server.on("/api/reset", handleReset);
  server.begin();

  if (cameraOk) {
    startCameraServer();
    Serial.printf("model %dx%d, %d labels\n", IN_W, IN_H, NLABELS);
    // core 1 (Wi-Fi and the camera server tasks live on core 0)
    xTaskCreatePinnedToCore(inferTask, "infer", 16384, NULL, 1, NULL, 1);
  }
}

void loop() {
  static uint32_t lastPrint = 0;
  server.handleClient();
  if (millis() - lastPrint >= 10000) {
    lastPrint = millis();
    Serial.printf("IP %s | cam %s | viewers %d | heap %u | psram %u\n",
                  WiFi.localIP().toString().c_str(), cameraOk ? "ok" : "NONE", cameraViewers(),
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
  }
  delay(2);
}
