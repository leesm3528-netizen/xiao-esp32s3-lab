// XIAO ESP32-S3: touch D0 (GPIO1, TOUCH1) -> USER_LED GPIO21 (LOW = ON)
// ESP32-S3 touchRead() value increases when touched
#define LED_PIN   21
#define TOUCH_PIN 1

uint32_t baseline;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // LED off

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

void loop() {
  uint32_t value = touchRead(TOUCH_PIN);
  bool touched = value > baseline * 1.3;  // 30% above baseline

  // track baseline while not touched
  if (!touched) {
    baseline = (baseline * 15 + value) / 16;
  }

  digitalWrite(LED_PIN, touched ? LOW : HIGH);

  Serial.printf("baseline=%lu value=%lu touched=%d\n", baseline, value, touched);
  delay(50);
}
