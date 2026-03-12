#include <Arduino.h>

const int redLED = 25;
const int blueLED = 26;

void setup() {
  pinMode(redLED, OUTPUT);
  pinMode(blueLED, OUTPUT);
}

void loop() {
  digitalWrite(redLED, HIGH);
  digitalWrite(blueLED, LOW);
  delay(200);
  digitalWrite(redLED, LOW);
  delay(200);

  digitalWrite(blueLED, HIGH);
  delay(200);
  digitalWrite(blueLED, LOW);
  delay(200);
}