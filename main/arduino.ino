#include <SPI.h>
#include <RF24.h>
#include <Servo.h>

#define DEBUG_RX      true
#define DEBUG_CALC    true
#define DEBUG_MOTORS  true
#define DEBUG_DIR     true
#define DEBUG_HUM     true
#define DEBUG_NONE    false

RF24 radio(9, 10);
const byte address[6] = "00001";

Servo escL, escR;
const int ESC_L_PIN    = 6;
const int ESC_R_PIN    = 7;
const int FAILSAFE_MS  = 500;
const int CENTER_X     = 1830;
const int CENTER_Y     = 1850;
const int DEADZONE     = 150;
const int HUMIDITY_PIN = A0;

struct DataIn {
  int16_t x;
  int16_t y;
  bool    emergency;
} __attribute__((packed));

struct DataAck {
  int16_t humidity;
} __attribute__((packed));

DataIn  data;
DataAck ackData;

unsigned long lastRX   = 0;
bool failsafeActive    = false;

int joystickToSpeed(int raw, int center) {
  int offset = raw - center;
  if (abs(offset) < DEADZONE) return 0;
  if (offset < 0) return map(offset, -center, -DEADZONE, -100, 0);
  else            return map(offset, DEADZONE, 4095 - center, 0, 100);
}

int speedToMicros(int speed) {
  return constrain(map(speed, -100, 100, 1000, 2000), 1000, 2000);
}

void emergencyStop() {
  // escL.writeMicroseconds(1000);
  // escR.writeMicroseconds(1000);
  if (!DEBUG_NONE) Serial.println("[URGENCE] STOP moteurs");
}

void setup() {
  Serial.begin(115200);

  // escL.attach(ESC_L_PIN);
  // escR.attach(ESC_R_PIN);
  // escL.writeMicroseconds(1000);
  // escR.writeMicroseconds(1000);
  // delay(2000);
  // escL.writeMicroseconds(1500);
  // escR.writeMicroseconds(1500);
  // delay(3000);

  if (!DEBUG_NONE) Serial.println("[OK] ESC armed (simulation)");

  if (!radio.begin()) {
    Serial.println("[ERR] radio FAIL");
    while(1) { delay(100); }
  }
  radio.setPALevel(RF24_PA_MIN);
  radio.enableAckPayload();
  radio.openReadingPipe(1, address);
  radio.startListening();
  if (!DEBUG_NONE) Serial.println("[OK] listening");
}

void loop() {
  ackData.humidity = (int16_t)analogRead(HUMIDITY_PIN);
  radio.writeAckPayload(1, &ackData, sizeof(ackData));

  if (radio.available()) {
    radio.read(&data, sizeof(data));
    lastRX        = millis();
    failsafeActive = false;

    // Arrêt d'urgence prioritaire
    if (data.emergency) {
      emergencyStop();
      return;
    }

    int speedY = -joystickToSpeed(data.x, CENTER_X);
    int speedX = -joystickToSpeed(data.y, CENTER_Y);
    int speedL = constrain(speedY + speedX, -100, 100);
    int speedR = constrain(speedY - speedX, -100, 100);
    int microsL = speedToMicros(speedL);
    int microsR = speedToMicros(speedR);

    if (!DEBUG_NONE && DEBUG_RX) {
      Serial.print("[RX] rawX="); Serial.print(data.x);
      Serial.print(" rawY="); Serial.println(data.y);
    }
    if (!DEBUG_NONE && DEBUG_HUM) {
      Serial.print("[HUM RAW] "); Serial.println(ackData.humidity);
    }
    if (!DEBUG_NONE && DEBUG_CALC) {
      Serial.print("[CALC] speedY="); Serial.print(speedY);
      Serial.print(" speedX="); Serial.println(speedX);
    }
    if (!DEBUG_NONE && DEBUG_MOTORS) {
      Serial.print("[MOTORS] L="); Serial.print(speedL);
      Serial.print("% ("); Serial.print(microsL);
      Serial.print("µs)  R="); Serial.print(speedR);
      Serial.print("% ("); Serial.print(microsR);
      Serial.println("µs)");
    }
    if (!DEBUG_NONE && DEBUG_DIR) {
      if      (speedY == 0 && speedX == 0)      Serial.println("[DIR] IMMOBILE");
      else if (speedY > 0  && abs(speedX) < 20) Serial.println("[DIR] AVANCE");
      else if (speedY < 0  && abs(speedX) < 20) Serial.println("[DIR] RECULE");
      else if (speedX > 0  && abs(speedY) < 20) Serial.println("[DIR] DROITE");
      else if (speedX < 0  && abs(speedY) < 20) Serial.println("[DIR] GAUCHE");
      else                                       Serial.println("[DIR] MIXTE");
    }

    // escL.writeMicroseconds(microsL);
    // escR.writeMicroseconds(microsR);

  } else {
    if (millis() - lastRX > FAILSAFE_MS) {
      if (!failsafeActive) {
        emergencyStop();
        failsafeActive = true;
        if (!DEBUG_NONE) Serial.println("[FAILSAFE] RF lost -> stop");
      }
    }
  }
}
