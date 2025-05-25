#include <SoftwareSerial.h>

SoftwareSerial BTSerial(PB7, PB6);  // Bluetooth RX, TX

// FL motor
#define FL_PWM PA8
#define FL_IN1 PA0
#define FL_IN2 PA1

// FR motor
#define FR_PWM PA9
#define FR_IN1 PA2
#define FR_IN2 PA3

// BL motor
#define BL_PWM PA10
#define BL_IN1 PA4
#define BL_IN2 PA5

// BR motor
#define BR_PWM PB0
#define BR_IN1 PA6
#define BR_IN2 PA7

#define LED_PIN PC13

int speedFactor = 0;  // Default speed
bool pivotMode = false;

unsigned long lastActivityTime = 0;
bool failSafeActive = false;

unsigned long lastBlinkTime = 0;
bool ledState = LOW;

void setup() {
  BTSerial.begin(9600);  // Start Bluetooth serial
  BTSerial.println("RC Booted...");

  pinMode(FL_PWM, OUTPUT); pinMode(FL_IN1, OUTPUT); pinMode(FL_IN2, OUTPUT);
  pinMode(FR_PWM, OUTPUT); pinMode(FR_IN1, OUTPUT); pinMode(FR_IN2, OUTPUT);
  pinMode(BL_PWM, OUTPUT); pinMode(BL_IN1, OUTPUT); pinMode(BL_IN2, OUTPUT);
  pinMode(BR_PWM, OUTPUT); pinMode(BR_IN1, OUTPUT); pinMode(BR_IN2, OUTPUT);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  lastActivityTime = millis();
}

void setMotor(int pwmPin, int in1, int in2, int speed) {
  if (speed > 0) {
    digitalWrite(in1, HIGH); digitalWrite(in2, LOW);
  } else if (speed < 0) {
    digitalWrite(in1, LOW); digitalWrite(in2, HIGH);
  } else {
    digitalWrite(in1, LOW); digitalWrite(in2, LOW); // Brake
  }
  analogWrite(pwmPin, abs(speed));
}

void driveMecanum(int x, int y, int rot) {
  int FL = (y + x + rot) * speedFactor / 255;
  int FR = (y - x - rot) * speedFactor / 255;
  int BL = (y - x + rot) * speedFactor / 255;
  int BR = (y + x - rot) * speedFactor / 255;

  FL = constrain(FL, -255, 255);
  FR = constrain(FR, -255, 255);
  BL = constrain(BL, -255, 255);
  BR = constrain(BR, -255, 255);

  setMotor(FL_PWM, FL_IN1, FL_IN2, FL);
  setMotor(FR_PWM, FR_IN1, FR_IN2, FR);
  setMotor(BL_PWM, BL_IN1, BL_IN2, BL);
  setMotor(BR_PWM, BR_IN1, BR_IN2, BR);
}

void stopMotors() {
  setMotor(FL_PWM, FL_IN1, FL_IN2, 0);
  setMotor(FR_PWM, FR_IN1, FR_IN2, 0);
  setMotor(BL_PWM, BL_IN1, BL_IN2, 0);
  setMotor(BR_PWM, BR_IN1, BR_IN2, 0);
}

void loop() {
  unsigned long currentTime = millis();

  if (BTSerial.available()) {
    char cmd = BTSerial.read();
    cmd = toupper(cmd);
    lastActivityTime = currentTime;
    failSafeActive = false;
    digitalWrite(LED_PIN, HIGH);  // LED steady ON when active

    if (cmd == 'F') driveMecanum(0, 255, 0);
    else if (cmd == 'B') driveMecanum(0, -255, 0);
    else if (cmd == 'R') {
      if (pivotMode) driveMecanum(0, 0, 255);
      else driveMecanum(-255, 0, 0);
    }
    else if (cmd == 'L') {
      if (pivotMode) driveMecanum(0, 0, -255);
      else driveMecanum(255, 0, 0);
    }
    else if (cmd == 'S') driveMecanum(0, 0, 0);
    else if (cmd == 'I') driveMecanum(-255, 255, 0);
    else if (cmd == 'G') driveMecanum(255, 255, 0);
    else if (cmd == 'J') driveMecanum(-255, -255, 0);
    else if (cmd == 'H') driveMecanum(255, -255, 0);

    // Speed levels remapped
    else if (cmd == '0') speedFactor = 0;
    else if (cmd == '1') speedFactor = 75;
    else if (cmd == '2') speedFactor = 77;
    else if (cmd == '3') speedFactor = 82;
    else if (cmd == '4') speedFactor = 90;
    else if (cmd == '5') speedFactor = 104;
    else if (cmd == '6') speedFactor = 120;
    else if (cmd == '7') speedFactor = 139;
    else if (cmd == '8') speedFactor = 163;
    else if (cmd == '9') speedFactor = 190;
    else if (cmd == 'Q') speedFactor = 255;


    // Pivot toggle
    else if (cmd == 'X') pivotMode = !pivotMode;
  } else {
    // No data received, check fail-safe timeout
    if (currentTime - lastActivityTime > 200) {
      if (!failSafeActive) {
        failSafeActive = true;
        stopMotors();
        lastBlinkTime = currentTime;
        ledState = LOW;
      }

      // Blink LED every 250ms
      if (currentTime - lastBlinkTime >= 250) {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
        lastBlinkTime = currentTime;
      }
    }
  }
}
