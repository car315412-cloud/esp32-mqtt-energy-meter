const int PIR_PIN = 14;
const int RED_PIN = 4;
const int GREEN_PIN = 15;

void setState(bool motion) {
  digitalWrite(RED_PIN, motion ? HIGH : LOW);
  digitalWrite(GREEN_PIN, motion ? LOW : HIGH);
}

void setup() {
  pinMode(PIR_PIN, INPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  setState(false);
}

void loop() {
  bool motion = digitalRead(PIR_PIN) == HIGH;
  setState(motion);
  delay(50);
}
