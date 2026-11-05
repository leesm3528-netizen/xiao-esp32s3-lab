// XIAO ESP32-S3 Sense: Edge Impulse image classification (Mint / WaterBottle)
// - camera stays JPEG 240x240 (same as the training photos), so the stream keeps working
// - inference runs in its own task: JPEG -> RGB888 -> 96x96 -> run_classifier()
// - port 80: /  (live view + result), /api/result (JSON)
// - port 81: /stream, /capture  (camera_stream.h)
// - serial: one line per inference; LED (GPIO21, active-low) on while LED_LABEL is detected
// library: EIModel/ei-mint-waterbottle-arduino-v1.zip (see EIModel/README.md)
// build: arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi EIInference
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

#define LED_PIN 21                     // active-low
const char *LED_LABEL = "WaterBottle";  // LED on when this label wins
const float LED_THRESHOLD = 0.80f;     // ...with at least this score
const uint32_t INFER_INTERVAL_MS = 200;

const int MAX_W = 640, MAX_H = 480;    // camera buffers are sized for VGA
static uint8_t *rgbBuf = nullptr;      // RGB888, decoded frame, then resized in place to 96x96

struct Result {
  float scores[EI_CLASSIFIER_LABEL_COUNT];
  int top;
  int dspMs, nnMs, totalMs;
  uint32_t count;
  uint32_t at;   // millis() of the result
  bool ok;
};
static Result lastResult = {};
static portMUX_TYPE resultMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool ledOn = false;

WebServer server(80);

// EI asks for pixels as packed 0xRRGGBB floats; fmt2rgb888 writes BGR order
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
  int w = fb->width, h = fb->height;
  bool ok = w <= MAX_W && h <= MAX_H && fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgbBuf);
  esp_camera_fb_return(fb);
  if (!ok) return false;

  // center-crop to square and resize (training used "fit shortest side")
  ei::image::processing::crop_and_interpolate_rgb888(rgbBuf, w, h, rgbBuf,
      EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT);

  ei::signal_t signal;
  signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
  signal.get_data = &getData;
  ei_impulse_result_t res = {};
  if (run_classifier(&signal, &res, false) != EI_IMPULSE_OK) return false;

  r.top = 0;
  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
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
  uint32_t count = 0, fails = 0;
  for (;;) {
    uint32_t start = millis();
    Result r = {};
    if (inferOnce(r)) {
      r.count = ++count;
      portENTER_CRITICAL(&resultMux);
      lastResult = r;
      portEXIT_CRITICAL(&resultMux);

      ledOn = strcmp(ei_classifier_inferencing_categories[r.top], LED_LABEL) == 0 &&
              r.scores[r.top] >= LED_THRESHOLD;
      digitalWrite(LED_PIN, ledOn ? LOW : HIGH);

      Serial.printf("#%lu %s %.2f |", (unsigned long)count,
                    ei_classifier_inferencing_categories[r.top], r.scores[r.top]);
      for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
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

String resultJson() {
  Result r;
  portENTER_CRITICAL(&resultMux);
  r = lastResult;
  portEXIT_CRITICAL(&resultMux);

  String s = "{\"ok\":";
  s += r.ok ? "true" : "false";
  s += ",\"label\":\"";
  s += r.ok ? ei_classifier_inferencing_categories[r.top] : "";
  s += "\",\"score\":";
  s += r.ok ? String(r.scores[r.top], 3) : "0";
  s += ",\"scores\":{";
  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    if (i) s += ",";
    s += "\"";
    s += ei_classifier_inferencing_categories[i];
    s += "\":";
    s += String(r.scores[i], 3);
  }
  char tail[200];
  snprintf(tail, sizeof(tail),
           "},\"dsp_ms\":%d,\"nn_ms\":%d,\"total_ms\":%d,\"count\":%lu,\"age_ms\":%lu,"
           "\"led\":%s,\"led_label\":\"%s\",\"threshold\":%.2f,\"camera\":%s,\"viewers\":%d}",
           r.dspMs, r.nnMs, r.totalMs, (unsigned long)r.count,
           (unsigned long)(r.ok ? millis() - r.at : 0), ledOn ? "true" : "false",
           LED_LABEL, LED_THRESHOLD, cameraOk ? "true" : "false", cameraViewers());
  s += tail;
  return s;
}

void handleResult() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", resultJson());
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

  rgbBuf = (uint8_t *)ps_malloc(MAX_W * MAX_H * 3);
  cameraOk = rgbBuf && initCamera();
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
  server.begin();

  if (cameraOk) {
    startCameraServer();
    Serial.printf("model %dx%d, %d labels\n", EI_CLASSIFIER_INPUT_WIDTH,
                  EI_CLASSIFIER_INPUT_HEIGHT, EI_CLASSIFIER_LABEL_COUNT);
    // core 1 (Wi-Fi and the camera server tasks live on core 0)
    xTaskCreatePinnedToCore(inferTask, "infer", 16384, NULL, 1, NULL, 1);
  }
}

void loop() {
  server.handleClient();
  delay(2);
}
