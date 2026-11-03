// XIAO ESP32-S3 MQTT dashboard (local broker)
// - touch sensor D0 (GPIO1, TOUCH1), LED GPIO21 (active-low: LOW = ON)
// - broker: PC running start-broker.ps1 (MQTT_HOST:MQTT_PORT in secrets.h)
//
// Topics
//   xiao/status   "online" / "offline"            retained, offline = last will
//   xiao/info     {"ip":"..","rssi":-50}           retained, on connect
//   xiao/touch    {"touched":false,"value":17000,"baseline":17000}   every 200 ms + on change
//   xiao/touch/value  "17000"   xiao/touch/state "1"/"0"   (same timing, for phone widgets)
//   xiao/led      "1" / "0"                       retained, current LED state
//   xiao/led/set  "on" / "off" / "toggle"           board subscribes
//
// build: arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3 MqttDashboard
#include <WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

#define LED_PIN   21
#define TOUCH_PIN 1

const char *T_STATUS  = "xiao/status";
const char *T_INFO    = "xiao/info";
const char *T_TOUCH   = "xiao/touch";
const char *T_TOUCH_VALUE = "xiao/touch/value";  // "17000" (number only)
const char *T_TOUCH_STATE = "xiao/touch/state";  // "1" touched / "0"
const char *T_LED     = "xiao/led";
const char *T_LED_SET = "xiao/led/set";

WiFiClient net;
PubSubClient mqtt(net);
String clientId;

bool ledOn = false;
bool ledDirty = true;  // publish LED state when true
bool touched = false;
uint32_t baseline;
uint32_t touchValue;

void setLed(bool on) {
  ledOn = on;
  digitalWrite(LED_PIN, on ? LOW : HIGH);
  ledDirty = true;
}

void calibrateTouch() {
  for (int i = 0; i < 10; i++) {  // discard unstable readings after boot
    touchRead(TOUCH_PIN);
    delay(20);
  }
  uint64_t sum = 0;  // don't touch D0 during boot
  for (int i = 0; i < 20; i++) {
    sum += touchRead(TOUCH_PIN);
    delay(10);
  }
  baseline = sum / 20;
}

// returns true when the touched state changed
bool updateTouch() {
  bool prev = touched;
  touchValue = touchRead(TOUCH_PIN);
  touched = touchValue > baseline * 1.3;  // 30% above baseline
  if (!touched) baseline = (baseline * 15 + touchValue) / 16;  // follow drift
  return touched != prev;
}

void onMessage(char *topic, byte *payload, unsigned int len) {
  String msg;
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  msg.trim();
  msg.toLowerCase();
  Serial.printf("<- %s %s\n", topic, msg.c_str());
  if (strcmp(topic, T_LED_SET) != 0) return;
  if (msg == "on" || msg == "1") setLed(true);
  else if (msg == "off" || msg == "0") setLed(false);
  else if (msg == "toggle") setLed(!ledOn);
}

void publishTouch() {
  char buf[96];
  snprintf(buf, sizeof(buf), "{\"touched\":%s,\"value\":%lu,\"baseline\":%lu}",
           touched ? "true" : "false", (unsigned long)touchValue, (unsigned long)baseline);
  mqtt.publish(T_TOUCH, buf);
  // plain values for phone app widgets (slider/gauge/indicator can't parse JSON)
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)touchValue);
  mqtt.publish(T_TOUCH_VALUE, buf);
  mqtt.publish(T_TOUCH_STATE, touched ? "1" : "0");
}

// non-blocking-ish: one attempt (socket timeout 2 s) every 3 s
void ensureMqtt() {
  static uint32_t lastTry = 0;
  if (mqtt.connected() || WiFi.status() != WL_CONNECTED) return;
  if (lastTry && millis() - lastTry < 3000) return;
  lastTry = millis();

  Serial.printf("MQTT connecting to %s:%d ... ", MQTT_HOST, MQTT_PORT);
  if (mqtt.connect(clientId.c_str(), nullptr, nullptr, T_STATUS, 1, true, "offline")) {
    Serial.println("ok");
    mqtt.publish(T_STATUS, "online", true);
    char info[64];
    snprintf(info, sizeof(info), "{\"ip\":\"%s\",\"rssi\":%d}",
             WiFi.localIP().toString().c_str(), WiFi.RSSI());
    mqtt.publish(T_INFO, info, true);
    mqtt.subscribe(T_LED_SET);
    ledDirty = true;
  } else {
    Serial.printf("failed rc=%d\n", mqtt.state());
  }
}

void setup() {
  Serial.begin(115200);
  // USB CDC: if nobody reads COM port, prints would block loop() for seconds
  // (touch messages dropped to ~1/s, LED commands lagged). Drop output instead.
  Serial.setTxTimeoutMs(0);
  pinMode(LED_PIN, OUTPUT);
  setLed(false);
  calibrateTouch();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);  // modem sleep delays incoming MQTT commands by 100s of ms
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(250);
  Serial.printf("WiFi %s, IP %s\n", WiFi.status() == WL_CONNECTED ? "connected" : "not connected",
                WiFi.localIP().toString().c_str());

  clientId = "xiao-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);
  mqtt.setSocketTimeout(2);
  mqtt.setKeepAlive(10);
}

void loop() {
  static uint32_t lastTouch = 0, lastPub = 0, lastPrint = 0;

  ensureMqtt();
  mqtt.loop();

  if (millis() - lastTouch >= 50) {
    lastTouch = millis();
    bool changed = updateTouch();
    if (mqtt.connected() && (changed || millis() - lastPub >= 200)) {
      lastPub = millis();
      publishTouch();
    }
  }

  if (ledDirty && mqtt.connected()) {
    ledDirty = !mqtt.publish(T_LED, ledOn ? "1" : "0", true);
  }

  if (millis() - lastPrint >= 5000) {
    lastPrint = millis();
    Serial.printf("IP=%s mqtt=%d touch=%d(%lu/%lu) led=%d\n",
                  WiFi.localIP().toString().c_str(), mqtt.connected(),
                  touched, (unsigned long)touchValue, (unsigned long)baseline, ledOn);
  }
}
