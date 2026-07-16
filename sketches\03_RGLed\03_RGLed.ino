const int RED_PIN = 4;
const int YELLOW_PIN = 2;
const int GREEN_PIN = 15;

void allOff() {
  digitalWrite(RED_PIN, LOW);
  digitalWrite(YELLOW_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
}

void setup() {
  pinMode(RED_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  allOff();
}

void loop() {
  allOff();
  digitalWrite(RED_PIN, HIGH);
  delay(5000);

  allOff();
  digitalWrite(YELLOW_PIN, HIGH);
  delay(1000);

  allOff();
  digitalWrite(GREEN_PIN, HIGH);
  delay(3000);
}
