#include <Arduino.h>

const int redLED = 25;
const int blueLED = 26;
const int buttonExt = 27;
const int buttonBoot = 0;

int delayTime = 500;

bool isButtonPressed(int pin) {
  if (digitalRead(pin) == LOW) {
    delay(50);
    if (digitalRead(pin) == LOW) {
      while (digitalRead(pin) == LOW);
      return true;
    }
  }
  return false;
}

void setup() {
  pinMode(redLED, OUTPUT);
  pinMode(blueLED, OUTPUT);
  pinMode(buttonExt, INPUT_PULLUP);
  pinMode(buttonBoot, INPUT_PULLUP);
  Serial.begin(115200);
}

void loop() {
  if (isButtonPressed(buttonExt)) {
    delayTime = 200;
    Serial.println("Режим: швидке миготіння");
  }

  if (isButtonPressed(buttonBoot)) {
    delayTime = 800;
    Serial.println("Режим: повільне миготіння");
  }

  digitalWrite(redLED, HIGH);
  digitalWrite(blueLED, LOW);
  delay(delayTime);

  digitalWrite(redLED, LOW);
  digitalWrite(blueLED, HIGH);
  delay(delayTime);
}