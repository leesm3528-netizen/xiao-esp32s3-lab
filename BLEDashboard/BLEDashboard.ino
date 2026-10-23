// XIAO ESP32-S3 BLE: read touch sensor and control LED from a smartphone
// - LED (GPIO21, LOW = ON), touch sensor D0 (GPIO1, TOUCH1)
// - advertises as BLE_NAME with SERVICE_UUID
//   TOUCH_UUID  read/notify  "touched,value,baseline"  e.g. "0,16881,16874" (notify every 200ms + on change)
//   LED_UUID    read/write/notify  "1" = on, "0" = off
//               write accepts "1"/"0", "on"/"off", "toggle", or a single byte 0x01/0x00
// - phone: ble.html (Web Bluetooth, Android Chrome) or nRF Connect
// build: arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3 BLEDashboard
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLE2901.h>

#define LED_PIN   21
#define TOUCH_PIN 1

#define BLE_NAME     "XIAO-ESP32S3"
#define SERVICE_UUID "d429d54e-a936-468d-8afb-8bb144cdb6a9"
#define TOUCH_UUID   "cc6ffe3a-db7f-4895-8b62-6aefb65010e5"
#define LED_UUID     "6083a55f-c5c1-4375-9d96-053e54580212"

const uint32_t NOTIFY_MS = 200;

BLEServer *bleServer = NULL;
BLECharacteristic *touchChar = NULL;
BLECharacteristic *ledChar = NULL;

volatile bool connected = false;
bool wasConnected = false;
volatile bool ledChanged = false;  // set by BLE write, notified from loop()

bool ledOn = false;
bool touched = false;
uint32_t baseline;
uint32_t touchValue;

void setLed(bool on) {
  ledOn = on;
  digitalWrite(LED_PIN, on ? LOW : HIGH);
  ledChar->setValue(on ? "1" : "0");
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
  String v = String(touched ? 1 : 0) + "," + touchValue + "," + baseline;
  touchChar->setValue(v);
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *server) {
    connected = true;
  }
  void onDisconnect(BLEServer *server) {
    connected = false;
  }
};

class LedCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) {
    String v = c->getValue();
    if (v.length() == 1 && (v[0] == 0 || v[0] == 1)) {
      setLed(v[0] == 1);
    } else {
      v.trim();
      v.toLowerCase();
      if (v == "1" || v == "on") setLed(true);
      else if (v == "0" || v == "off") setLed(false);
      else if (v == "toggle") setLed(!ledOn);
      else c->setValue(ledOn ? "1" : "0");  // ignore unknown input, restore value
    }
    ledChanged = true;
  }
};

void setupBle() {
  BLEDevice::init(BLE_NAME);
  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  BLEService *service = bleServer->createService(SERVICE_UUID);

  touchChar = service->createCharacteristic(
    TOUCH_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  touchChar->addDescriptor(new BLE2902());
  BLE2901 *touchDesc = new BLE2901();
  touchDesc->setDescription("Touch: touched,value,baseline");
  touchDesc->setAccessPermissions(ESP_GATT_PERM_READ);
  touchChar->addDescriptor(touchDesc);

  ledChar = service->createCharacteristic(
    LED_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE |
              BLECharacteristic::PROPERTY_NOTIFY);
  ledChar->addDescriptor(new BLE2902());
  BLE2901 *ledDesc = new BLE2901();
  ledDesc->setDescription("LED: write 1/0, on/off, toggle");
  ledDesc->setAccessPermissions(ESP_GATT_PERM_READ);
  ledChar->addDescriptor(ledDesc);
  ledChar->setCallbacks(new LedCallbacks());

  service->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);  // name goes in the scan response (128-bit UUID fills the adv packet)
  BLEDevice::startAdvertising();
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  calibrateTouch();
  setupBle();
  setLed(false);
  updateTouch();
  Serial.printf("BLE \"%s\" advertising\n", BLE_NAME);
}

void loop() {
  static uint32_t lastTouch = 0;
  static uint32_t lastNotify = 0;
  static uint32_t lastPrint = 0;
  static bool lastTouched = false;

  if (millis() - lastTouch >= 50) {
    lastTouch = millis();
    updateTouch();
  }

  if (connected) {
    if (millis() - lastNotify >= NOTIFY_MS || touched != lastTouched) {
      lastNotify = millis();
      lastTouched = touched;
      touchChar->notify();
    }
    if (ledChanged) {
      ledChanged = false;
      ledChar->notify();
    }
  }

  // restart advertising after the phone disconnects
  if (!connected && wasConnected) {
    BLEDevice::startAdvertising();
    Serial.println("disconnected, advertising again");
  }
  if (connected && !wasConnected) {
    Serial.println("phone connected");
  }
  wasConnected = connected;

  if (millis() - lastPrint >= 5000) {
    lastPrint = millis();
    Serial.printf("BLE=%s connected=%d touch=%d(%lu/%lu) led=%d\n",
                  BLE_NAME, connected, touched, touchValue, baseline, ledOn);
  }
}
