// accelerometer citation: https://mschoeffler.com/2017/10/05/tutorial-how-to-use-the-gy-521-module-mpu-6050-breakout-board-with-the-arduino-uno/

#include <Wire.h>

#define MPU_ADDR 0x68
#define BUZZER 8
#define VRX A0
#define VRY A1

// === CONFIGURATION ===
#define MODE_GYRO 1        // 1 = GY-521 tilt control, 0 = Joystick
#define UPDATE_RATE 20
#define TILT_THRESHOLD 0.5
#define JOY_DEADZONE 50
#define JOY_CENTER 512
#define SHAKE_WINDOW 20
#define SHAKE_THRESHOLD 0.5 // shake
// =====================

int16_t AcX, AcY, AcZ, GyX, GyY, GyZ;
float accelBuffer[SHAKE_WINDOW][3];
int bufferIdx = 0;
bool bufferFull = false;
char lastDir = ' ';
bool shakeActive = false;
unsigned long lastUpdate = 0;

void setup() {
  Serial.begin(9600);
  pinMode(BUZZER, OUTPUT);

  if (MODE_GYRO) {
    initMPU();
  } else {
    pinMode(VRX, INPUT);
    pinMode(VRY, INPUT);
  }

  // Initialize shake buffer
  for (int i = 0; i < SHAKE_WINDOW; i++) {
    accelBuffer[i][0] = accelBuffer[i][1] = accelBuffer[i][2] = 0;
  }

  while (!Serial) {};
}

void loop() {
  if (millis() - lastUpdate < (1000 / UPDATE_RATE)) return;
  lastUpdate = millis();

  char dir = ' ';

  if (MODE_GYRO) {
    readMPU();

    float ax = AcX / 16384.0;
    float ay = AcY / 16384.0;
    float az = AcZ / 16384.0;

    if (abs(ay) > TILT_THRESHOLD) {
      dir = (ay > 0) ? 's' : 'w';
    } else if (abs(ax) > TILT_THRESHOLD) {
      dir = (ax > 0) ? 'a' : 'd';
    }

    // === SHAKE DETECTION ===
    accelBuffer[bufferIdx][0] = ax * ax;
    accelBuffer[bufferIdx][1] = ay * ay;
    accelBuffer[bufferIdx][2] = az * az;

    bufferIdx = (bufferIdx + 1) % SHAKE_WINDOW;
    if (!bufferFull && bufferIdx == 0) bufferFull = true;

    if (bufferFull && !shakeActive) {
      float varX = variance(0);
      float varY = variance(1);
      float varZ = variance(2);

      if (varX + varY + varZ > SHAKE_THRESHOLD) {
        Serial.println('S');
        shakeActive = true;
      }
    }

    if (shakeActive && sqrt(ax * ax + ay * ay + az * az) < 1.2) {
      shakeActive = false; // Reset after settle
    }

  } else {
    // === JOYSTICK MODE ===
    int x = analogRead(VRX);
    int y = analogRead(VRY);

    if (abs(y - JOY_CENTER) > JOY_DEADZONE || abs(x - JOY_CENTER) > JOY_DEADZONE) {
      if (abs(y - JOY_CENTER) > abs(x - JOY_CENTER)) {
        dir = (y < JOY_CENTER) ? 'w' : 's';
      } else {
        dir = (x < JOY_CENTER) ? 'a' : 'd';
      }
    }
  }

  // Send direction only on change
  if (dir != ' ' && dir != lastDir) {
    Serial.println(dir);
    lastDir = dir;
  }

  // === BUZZER IN ===
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'E') {
      tone(BUZZER, 1500, 250);
      break;
    }
  }
}

void initMPU() {
  Wire.begin();
  Wire.setClock(400000);   // I2C fast mode
  writeReg(0x6B, 0x00);    // Wake up
  writeReg(0x1C, 0x00);    // Accel sensitivity +/-2g
  writeReg(0x1B, 0x00);    // Gyro sensitivity +/-250dps
  delay(100);
}

void writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission(true);
}

void readMPU() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);

  AcX = Wire.read() << 8 | Wire.read();
  AcY = Wire.read() << 8 | Wire.read();
  AcZ = Wire.read() << 8 | Wire.read();
  Wire.read(); Wire.read(); // Temp
  GyX = Wire.read() << 8 | Wire.read();
  GyY = Wire.read() << 8 | Wire.read();
  GyZ = Wire.read() << 8 | Wire.read();
}

float variance(int axis) {
  float mean = 0, sumSq = 0;
  for (int i = 0; i < SHAKE_WINDOW; i++) {
    float val = accelBuffer[i][axis];
    mean += val;
    sumSq += val * val;
  }
  mean /= SHAKE_WINDOW;
  return (sumSq / SHAKE_WINDOW) - (mean * mean);
}
