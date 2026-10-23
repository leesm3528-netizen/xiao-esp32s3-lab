// XIAO ESP32-S3 web dashboard
// - LED (GPIO21, LOW = ON) on/off from web
// - touch sensor D0 (GPIO1, TOUCH1) state shown on web
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "secrets.h"

#define LED_PIN   21
#define TOUCH_PIN 1

WebServer server(80);

bool ledOn = false;
bool touched = false;
uint32_t baseline;
uint32_t touchValue;

const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="ko">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>XIAO ESP32-S3</title>
<style>
  :root { --bg:#f4f5f7; --card:#fff; --text:#1d1f23; --muted:#6b7280; --on:#f59e0b; --touch:#10b981; --off:#d1d5db; }
  @media (prefers-color-scheme: dark) {
    :root { --bg:#111317; --card:#1b1e24; --text:#e8eaed; --muted:#9aa0a6; --off:#3a3f47; }
  }
  * { box-sizing:border-box; }
  body { margin:0; font-family:system-ui,-apple-system,"Malgun Gothic",sans-serif; background:var(--bg); color:var(--text); }
  main { max-width:480px; margin:0 auto; padding:24px 16px; }
  h1 { font-size:20px; margin:0 0 4px; }
  .sub { color:var(--muted); font-size:13px; margin-bottom:20px; }
  .card { background:var(--card); border-radius:14px; padding:20px; margin-bottom:16px; box-shadow:0 1px 3px rgba(0,0,0,.08); }
  .row { display:flex; align-items:center; justify-content:space-between; }
  .label { font-size:15px; font-weight:600; }
  .state { font-size:13px; color:var(--muted); margin-top:4px; }
  .dot { width:56px; height:56px; border-radius:50%; background:var(--off); transition:background .15s, box-shadow .15s; }
  .dot.led { background:var(--on); box-shadow:0 0 24px var(--on); }
  .dot.touch { background:var(--touch); box-shadow:0 0 24px var(--touch); }
  .btns { display:flex; gap:10px; margin-top:16px; }
  button { flex:1; padding:12px; font-size:15px; font-weight:600; border:0; border-radius:10px; cursor:pointer; background:var(--off); color:var(--text); }
  button.primary { background:var(--on); color:#fff; }
  .meta { font-size:12px; color:var(--muted); margin-top:12px; font-variant-numeric:tabular-nums; }
  .err { color:#ef4444; }
</style>
</head>
<body>
<main>
  <h1>XIAO ESP32-S3 대시보드</h1>
  <div class="sub" id="conn">연결 중…</div>

  <div class="card">
    <div class="row">
      <div><div class="label">LED (GPIO21)</div><div class="state" id="ledState">-</div></div>
      <div class="dot" id="ledDot"></div>
    </div>
    <div class="btns">
      <button class="primary" onclick="setLed(1)">켜기</button>
      <button onclick="setLed(0)">끄기</button>
    </div>
  </div>

  <div class="card">
    <div class="row">
      <div><div class="label">터치센서 (D0)</div><div class="state" id="touchState">-</div></div>
      <div class="dot" id="touchDot"></div>
    </div>
    <div class="meta" id="touchMeta">-</div>
  </div>
</main>
<script>
function render(s) {
  document.getElementById('ledState').textContent = s.led ? '켜짐' : '꺼짐';
  document.getElementById('ledDot').className = 'dot' + (s.led ? ' led' : '');
  document.getElementById('touchState').textContent = s.touched ? '터치 중' : '터치 안 함';
  document.getElementById('touchDot').className = 'dot' + (s.touched ? ' touch' : '');
  document.getElementById('touchMeta').textContent = '값 ' + s.value + ' / 기준 ' + s.baseline;
  const c = document.getElementById('conn');
  c.textContent = '연결됨 · ' + location.host; c.className = 'sub';
}
function fail() {
  const c = document.getElementById('conn');
  c.textContent = '연결 끊김'; c.className = 'sub err';
}
async function poll() {
  try { render(await (await fetch('/api/status')).json()); } catch (e) { fail(); }
  setTimeout(poll, 200);
}
async function setLed(on) {
  try { render(await (await fetch('/api/led?state=' + (on ? 'on' : 'off'), { method: 'POST' })).json()); } catch (e) { fail(); }
}
poll();
</script>
</body>
</html>)HTML";

void setLed(bool on) {
  ledOn = on;
  digitalWrite(LED_PIN, on ? LOW : HIGH);
}

String statusJson() {
  return String("{\"led\":") + (ledOn ? "true" : "false") +
         ",\"touched\":" + (touched ? "true" : "false") +
         ",\"value\":" + touchValue +
         ",\"baseline\":" + baseline + "}";
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
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
  } else {
    server.send(400, "application/json", "{\"error\":\"state must be on or off\"}");
    return;
  }
  server.send(200, "application/json", statusJson());
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

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  setLed(false);

  calibrateTouch();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("xiao")) {
    MDNS.addService("http", "tcp", 80);
  }

  server.on("/", handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/led", HTTP_POST, handleLed);
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

  if (millis() - lastPrint >= 2000) {
    lastPrint = millis();
    Serial.printf("IP=%s led=%d touched=%d value=%lu baseline=%lu\n",
                  WiFi.localIP().toString().c_str(), ledOn, touched, touchValue, baseline);
  }
}
