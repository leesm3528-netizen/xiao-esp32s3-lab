// XIAO ESP32-S3: USER_LED = GPIO21 (LOW = ON)
#define LED_PIN 21

void setup() {
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_PIN, LOW);   // LED on
  digitalWrite(LED_PIN, HIGH);  // LED off
}
