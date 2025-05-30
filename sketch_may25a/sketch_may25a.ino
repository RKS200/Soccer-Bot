#include <SoftwareSerial.h>
#include <Adafruit_NeoPixel.h>

SoftwareSerial BTSerial(PB7, PB6);

// Motor Pins
#define FL_PWM PB9
#define FL_IN1 PA7
#define FL_IN2 PA6

#define FR_PWM PA10
#define FR_IN1 PA5
#define FR_IN2 PA4

#define BL_PWM PA8
#define BL_IN1 PA0
#define BL_IN2 PA1

#define BR_PWM PA9
#define BR_IN1 PA2
#define BR_IN2 PA3


#define LED_PIN PC13
#define BUZZER_PIN PC14

#define LED_PIN_WS PB10
#define NUM_LEDS 8
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN_WS, NEO_GRB + NEO_KHZ800);

// Globals
int speedFactor = 0;
bool pivotMode = false;
unsigned long lastActivityTime = 0;
bool failSafeActive = false;
unsigned long lastBlinkTime = 0;
bool ledState = LOW;
int failPixel = 0;
unsigned long lastLEDAnimTime = 0;

int motorSpeeds[4] = {0, 0, 0, 0};  // FL, FR, BL, BR

char lastCmd = 'S';
unsigned long lastUpdateTime = 0;
int isHorn = 0;
int flip = 1;

void setup() {
  BTSerial.begin(9600);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(FL_PWM, OUTPUT); pinMode(FL_IN1, OUTPUT); pinMode(FL_IN2, OUTPUT);
  pinMode(FR_PWM, OUTPUT); pinMode(FR_IN1, OUTPUT); pinMode(FR_IN2, OUTPUT);
  pinMode(BL_PWM, OUTPUT); pinMode(BL_IN1, OUTPUT); pinMode(BL_IN2, OUTPUT);
  pinMode(BR_PWM, OUTPUT); pinMode(BR_IN1, OUTPUT); pinMode(BR_IN2, OUTPUT);

  strip.begin(); strip.show();

 // Startup fade-in light blue animation with increasing buzzer frequency
for (int brightness = 0; brightness <= 255; brightness++) {
  strip.clear();
  uint32_t color = strip.Color(0, brightness, brightness / 2);  // light blue tint
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();

  int freq = 1000 + (brightness * 5);  // frequency increases with brightness
  tone(BUZZER_PIN, freq);

  delay(10);  // 10 ms delay for smooth fade
}
  strip.clear(); strip.show(); noTone(BUZZER_PIN);
  lastActivityTime = millis();
}

void setMotor(int pwm, int in1, int in2, int val) {
  if (val > 0) {
    digitalWrite(in1, HIGH); digitalWrite(in2, LOW);
  } else if (val < 0) {
    digitalWrite(in1, LOW); digitalWrite(in2, HIGH);
  } else {
    digitalWrite(in1, LOW); digitalWrite(in2, LOW);
  }
  analogWrite(pwm, abs(val));
}

void driveMecanum(int x, int y) {
  int FL = (y - x) * speedFactor/ 255;
  int FR = (y + x) * speedFactor/ 255;
  int BL = (y + x) * speedFactor/ 255;
  int BR = (y - x) * speedFactor/ 255;

  motorSpeeds[0] = constrain(FL, -255, 255);
  motorSpeeds[1] = constrain(FR, -255, 255);
  motorSpeeds[2] = constrain(BL, -255, 255);
  motorSpeeds[3] = constrain(BR, -255, 255);

  setMotor(FL_PWM, FL_IN1, FL_IN2, motorSpeeds[0]);
  setMotor(FR_PWM, FR_IN1, FR_IN2, motorSpeeds[1]);
  setMotor(BL_PWM, BL_IN1, BL_IN2, motorSpeeds[2]);
  setMotor(BR_PWM, BR_IN1, BR_IN2, motorSpeeds[3]);
}

void driveSimple(int x, int y) {
  // y controls forward/backward
  // x controls turning
  int left = (y + x) * speedFactor / 255;
  int right = (y - x) * speedFactor/ 255;

  motorSpeeds[0] = constrain(left, -255, 255);   // FL
  motorSpeeds[1] = constrain(right, -255, 255);  // FR
  motorSpeeds[2] = constrain(left, -255, 255);   // BL
  motorSpeeds[3] = constrain(right, -255, 255);  // BR

  setMotor(FL_PWM, FL_IN1, FL_IN2, motorSpeeds[0]);
  setMotor(FR_PWM, FR_IN1, FR_IN2, motorSpeeds[1]);
  setMotor(BL_PWM, BL_IN1, BL_IN2, motorSpeeds[2]);
  setMotor(BR_PWM, BR_IN1, BR_IN2, motorSpeeds[3]);
}

void updateMotorLEDs(char cmd) {
  strip.clear();
  strip.setPixelColor((0*2)+1, strip.Color(75*(motorSpeeds[0] < 0), 75*(motorSpeeds[0] < 0), 150*(motorSpeeds[0] > 0)));
  strip.setPixelColor((2*2)+1, strip.Color(75*(motorSpeeds[1] < 0), 75*(motorSpeeds[1] < 0), 150*(motorSpeeds[1] > 0)));
  strip.setPixelColor((1*2)+1, strip.Color(75*(motorSpeeds[2] < 0), 75*(motorSpeeds[2] < 0), 150*(motorSpeeds[2] > 0)));
  strip.setPixelColor((3*2)+1, strip.Color(75*(motorSpeeds[3] < 0), 75*(motorSpeeds[3] < 0), 150*(motorSpeeds[3] > 0)));
  strip.show();
  lastCmd = cmd;
}

void stopMotors() {
  for (int i = 0; i < 4; i++) motorSpeeds[i] = 0;
  setMotor(FL_PWM, FL_IN1, FL_IN2, 0);
  setMotor(FR_PWM, FR_IN1, FR_IN2, 0);
  setMotor(BL_PWM, BL_IN1, BL_IN2, 0);
  setMotor(BR_PWM, BR_IN1, BR_IN2, 0);
}

void loop() {
  unsigned long now = millis();

  if (BTSerial.available()) {
    char cmd = BTSerial.read();
    lastActivityTime = now;
    failSafeActive = false;
    digitalWrite(LED_PIN, HIGH);
    if (isHorn == 0){
      noTone(BUZZER_PIN);
    }
    else{
      tone(BUZZER_PIN, 2500, 100);
    }

    if (cmd == 'F') pivotMode ? driveSimple(0, 255) : driveMecanum(0, 255);
    else if (cmd == 'B') pivotMode ? driveSimple(0, -255) : driveMecanum(0, -255);
    else if (cmd == 'R') pivotMode ? driveSimple(200, 0) : driveMecanum(255, 0);
    else if (cmd == 'L') pivotMode ? driveSimple(-200, 0) : driveMecanum(-255, 0);
    else if (cmd == 'S') pivotMode ? driveSimple(0, 0) : driveMecanum(0, 0);
    else if (cmd == 'I') pivotMode ? driveSimple(255, 255) : driveMecanum(255, 255);
    else if (cmd == 'G') pivotMode ? driveSimple(-255, 255) : driveMecanum(-255, 255);
    else if (cmd == 'J') pivotMode ? driveSimple(255, -255) : driveMecanum(255, -255);
    else if (cmd == 'H') pivotMode ? driveSimple(-255, -255) : driveMecanum(-255, -255);
    else if (cmd >= '0' && cmd <= '9') speedFactor = map(cmd - '0', 0, 9, 0, 190);
    else if (cmd == 'q') speedFactor = 255;
    else if (cmd == 'X') pivotMode = 1;
    else if (cmd == 'x') pivotMode = 0;
    else if (cmd == 'V') isHorn = 1;
    else if (cmd == 'v') isHorn = 0;
    

    if (((millis() - lastUpdateTime) > 100) && lastCmd != cmd){
      updateMotorLEDs(cmd);
      lastUpdateTime = millis();
    }
  }

  // Fail-safe check
  if (now - lastActivityTime > 200) {
    if (!failSafeActive) {
      failSafeActive = true;
      stopMotors();
      lastBlinkTime = now;
      failPixel = 0;
    }

    // Blink LED + buzzer
    if (now - lastBlinkTime > 750) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      tone(BUZZER_PIN, 2500, 100);
      lastBlinkTime = now;
    }
    if (now - lastBlinkTime > 750) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      tone(BUZZER_PIN, 2500, 100);
      lastBlinkTime = now;
    }

    // Red LED sweep
    if (now - lastLEDAnimTime > 750) {
      strip.clear();
      for (int j = 0; j < 8; j++){
      strip.setPixelColor(j, strip.Color(125*failPixel%2, 0, 0));
    }
      strip.show();
      failPixel = (failPixel + 1) % NUM_LEDS;
      lastLEDAnimTime = now;
    }
}
}
