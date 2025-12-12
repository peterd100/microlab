#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal.h>
#define IR_USE_AVR_TIMER3
#include <IRremote.hpp>

const uint8_t IR_PIN = 10;
const uint8_t SOUND_PIN = A0;
const uint8_t BUTTON_PIN = 6;
const uint8_t MOTOR_EN = 9;
const uint8_t MOTOR_DIRA = 7;
const uint8_t MOTOR_DIRB = 8;
const int BASELINE = 36;

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
RTC_DS1307 rtc;

// Display time (updated in ISR)
volatile uint8_t displayHours = 0;
volatile uint8_t displayMinutes = 0;
volatile uint8_t displaySeconds = 0;

// Motor state
bool motorRunning = true;
bool clockwise = true;
uint8_t speedLevel = 0;
const uint8_t pwmValues[5] = {0, 64, 128, 192, 255};

// Time setting
bool timeSetMode = false;
uint8_t timeSetIndex = 0;
uint8_t timeSetDigits[6] = {0};

// IR codes
#define IR_PLAY_PAUSE 0x40
#define IR_FF         0x43
#define IR_REW        0x44
#define IR_FUNC_STOP  0x47
#define IR_0 0x16
#define IR_1 0x0C
#define IR_2 0x18
#define IR_3 0x5E
#define IR_4 0x08
#define IR_5 0x1C
#define IR_6 0x5A
#define IR_7 0x42
#define IR_8 0x52
#define IR_9 0x4A

// Button debounce
uint8_t buttonState = HIGH;
uint8_t lastButtonReading = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Manual override
uint32_t lastManualTime = 0;
const uint32_t MANUAL_OVERRIDE_TIME = 3000;

// 1-second tick flag
volatile bool secondTick = false;

void applyMotor();
void updateDisplay(bool forceFull = false);

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_EN, OUTPUT);
  pinMode(MOTOR_DIRA, OUTPUT);
  pinMode(MOTOR_DIRB, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("Lab 3 Starting..");
  delay(2000);
  lcd.clear();

  if (!rtc.begin()) while (1);
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  DateTime now = rtc.now();
  displayHours   = now.hour();
  displayMinutes = now.minute();
  displaySeconds = now.second();

  // Timer1
  cli();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;
  OCR1A = 15624;                            // 16 MHz / 1024 / 1 Hz - 1
  TCCR1B |= (1 << WGM12);                   // CTC mode
  TCCR1B |= (1 << CS12) | (1 << CS10);      // 1024 prescaler
  TIMSK1 |= (1 << OCIE1A);                  // Enable compare interrupt
  sei();

  applyMotor();
  updateDisplay(true);
}

ISR(TIMER1_COMPA_vect) {
  if (++displaySeconds >= 60) {
    displaySeconds = 0;
    if (++displayMinutes >= 60) {
      displayMinutes = 0;
      displayHours = (displayHours + 1) % 24;
    }
  }
  secondTick = true;   // Tell loop() a second has passed
}

void loop() {
  static uint32_t lastSoundTime = 0;

  // Update clock display every second
  if (secondTick) {
    secondTick = false;
    updateDisplay(false);
  }

  // Sound-based speed control
  if (millis() - lastManualTime >= MANUAL_OVERRIDE_TIME && millis() - lastSoundTime >= 30) {
    lastSoundTime = millis();
    long sum = 0;
    for (int i = 0; i < 128; i++) sum += analogRead(SOUND_PIN);
    int raw = sum >> 7;
    int signal = raw - BASELINE;
    if (signal < 0) signal = 0;

    if (motorRunning) {
      if (signal >= 20) speedLevel = 4;
      else if (signal >= 12) speedLevel = 3;
      else if (signal >= 7) speedLevel = 2;
      else if (signal >= 3) speedLevel = 1;
      else speedLevel = 0;
    } else {
      speedLevel = 0;
    }
    applyMotor();
    updateDisplay(false);
  }

  // Button - direction toggle
  uint8_t reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonReading) lastDebounceTime = millis();
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        clockwise = !clockwise;
        lastManualTime = millis();
        Serial.println(clockwise ? F("Direction -> CW") : F("Direction -> CCW"));
        applyMotor();
        updateDisplay(true);         
      }
    }
  }
  lastButtonReading = reading;

  // IR handling
  if (IrReceiver.decode()) {
    uint8_t cmd = IrReceiver.decodedIRData.command;

    // Skip repeat frames
    if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
      IrReceiver.resume();
      return;              
    }

    if (cmd == IR_FUNC_STOP) {
      timeSetMode = !timeSetMode;
      timeSetIndex = 0;
      memset(timeSetDigits, 0, sizeof(timeSetDigits));
      updateDisplay(true);
    }
    else if (timeSetMode) {
      uint8_t d = 255;
      switch (cmd) {
        case IR_0: d = 0; break;
        case IR_1: d = 1; break;
        case IR_2: d = 2; break;
        case IR_3: d = 3; break;
        case IR_4: d = 4; break;
        case IR_5: d = 5; break;
        case IR_6: d = 6; break;
        case IR_7: d = 7; break;
        case IR_8: d = 8; break;
        case IR_9: d = 9; break;
      }
      if (d <= 9) {
        timeSetDigits[timeSetIndex++] = d;
        uint8_t pos = 5;
        if (timeSetIndex == 2) pos = 6;
        if (timeSetIndex == 3) pos = 8;
        if (timeSetIndex == 4) pos = 9;
        if (timeSetIndex == 5) pos = 11;
        if (timeSetIndex == 6) pos = 12;
        lcd.setCursor(pos, 1);
        lcd.print(d);

        if (timeSetIndex == 6) {
          uint8_t h = timeSetDigits[0] * 10 + timeSetDigits[1];
          uint8_t m = timeSetDigits[2] * 10 + timeSetDigits[3];
          uint8_t s = timeSetDigits[4] * 10 + timeSetDigits[5];
          if (h < 24 && m < 60 && s < 60) {
            displayHours   = h;
            displayMinutes = m;
            displaySeconds = s;
            rtc.adjust(DateTime(2025, 11, 24, h, m, s));
          }
          timeSetMode = false;
          updateDisplay(true);
        }
      }
    }
    else {
      switch (cmd) {
        case IR_PLAY_PAUSE:
          motorRunning = !motorRunning;
          lastManualTime = millis();
          Serial.println(motorRunning ? F("Motor ON") : F("Motor OFF"));
          applyMotor();
          updateDisplay(false);
          break;
        case IR_FF:
          if (speedLevel < 4) { speedLevel++; lastManualTime = millis(); applyMotor(); updateDisplay(false); }
          break;
        case IR_REW:
          if (speedLevel > 0) { speedLevel--; lastManualTime = millis(); applyMotor(); updateDisplay(false); }
          break;
      }
    }
    IrReceiver.resume();
  }
}

void applyMotor() {
  analogWrite(MOTOR_EN, motorRunning ? pwmValues[speedLevel] : 0);
  digitalWrite(MOTOR_DIRA, clockwise ? HIGH : LOW);
  digitalWrite(MOTOR_DIRB, clockwise ? LOW : HIGH);
}

void updateDisplay(bool forceFull = false) {
  static uint8_t lastH = 255, lastM = 255, lastS = 255;
  static bool lastCW = !clockwise;
  static uint8_t lastSpeed = 255;

  // Time line
  if (forceFull || displayHours != lastH || displayMinutes != lastM || displaySeconds != lastS) {
    lcd.setCursor(0, 0);
    lcd.print(displayHours   < 10 ? "0" : ""); lcd.print(displayHours);   lcd.print(':');
    lcd.print(displayMinutes < 10 ? "0" : ""); lcd.print(displayMinutes); lcd.print(':');
    lcd.print(displaySeconds < 10 ? "0" : ""); lcd.print(displaySeconds);
    lastH = displayHours; lastM = displayMinutes; lastS = displaySeconds;
  }

  if (timeSetMode) {
    lcd.setCursor(0, 1);
    lcd.print("Set: __:__:__   ");
    if (timeSetIndex > 0) { lcd.setCursor(5, 1);  lcd.print(timeSetDigits[0]); }
    if (timeSetIndex > 1) { lcd.setCursor(6, 1);  lcd.print(timeSetDigits[1]); }
    if (timeSetIndex > 2) { lcd.setCursor(8, 1);  lcd.print(timeSetDigits[2]); }
    if (timeSetIndex > 3) { lcd.setCursor(9, 1);  lcd.print(timeSetDigits[3]); }
    if (timeSetIndex > 4) { lcd.setCursor(11, 1); lcd.print(timeSetDigits[4]); }
    if (timeSetIndex > 5) { lcd.setCursor(12, 1); lcd.print(timeSetDigits[5]); }
    return;
  }

  // Status line
  if (forceFull || clockwise != lastCW || speedLevel != lastSpeed) {
    lcd.setCursor(0, 1);
    lcd.print(clockwise ? "CW " : "CC ");
    switch (speedLevel) {
      case 0: lcd.print("Off   "); break;
      case 1: lcd.print("1/4   "); break;
      case 2: lcd.print("1/2   "); break;
      case 3: lcd.print("3/4   "); break;
      case 4: lcd.print("Full  "); break;
    }
    lcd.print("       ");        // Clear rest of line
    lastCW = clockwise;
    lastSpeed = speedLevel;
  }
}
