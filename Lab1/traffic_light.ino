#include <Keypad.h>

extern "C" {
  void start();
}

// Keypad configuration
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {22, 24, 26, 28}; // (PA0, PA2, PA4, PA6)
byte colPins[COLS] = {30, 32, 34, 36}; // (PC7, PC5, PC3, PC1)
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// LED and buzzer pins
#define NS_RED_PIN 23      // PA1 (North-South Red)
#define EW_RED_PIN 25      // PA3 (East-West Red)
#define NS_YELLOW_PIN 27   // PA5 (North-South Yellow)
#define EW_YELLOW_PIN 29   // PA7 (East-West Yellow)
#define NS_GREEN_PIN 31    // PC6 (North-South Green)
#define EW_GREEN_PIN 33    // PC4 (East-West Green)
#define BUZZER_PIN 35      // PC2 (Buzzer)

// 7-segment display configuration
byte numDigits = 2;
byte digitPins[] = {10, 11};     // DIG1, DIG2 
byte segmentPins[] = {2, 3, 4, 5, 6, 7, 8, 9}; // a, b, c, d, e, f, g, dp
const byte digitPatterns[10] = {
  0b00111111, // 0
  0b00000110, // 1
  0b01011011, // 2
  0b01001111, // 3
  0b01100110, // 4
  0b01101101, // 5
  0b01111101, // 6
  0b00000111, // 7
  0b01111111, // 8
  0b01101111  // 9
};

// State variables
volatile unsigned long nsRedDuration = 0;   // North-South Red duration in seconds
volatile unsigned long nsGreenDuration = 0; // North-South Green duration in seconds
volatile unsigned long ewRedDuration = 0;   // East-West Red duration in seconds
volatile unsigned long ewGreenDuration = 0; // East-West Green duration in seconds

volatile bool durationsSet = false;         // True when required durations are set
volatile bool sequenceStarted = false;      // True after '*' is pressed
volatile bool dualMode = false;             // True if dual-direction mode (5520# entered)

char inputBuffer[5];                        // Buffer for 5520#, A/B/C/D, two digits, #
byte inputIndex = 0;                        // Current position in inputBuffer

enum State { 
  INITIAL, 
  // Dual-direction states
  NS_RED, NS_RED_FLASH, NS_BUZZER, NS_GREEN, NS_GREEN_FLASH, NS_YELLOW, NS_TO_EW_BUZZER,
  EW_RED, EW_RED_FLASH, EW_BUZZER, EW_GREEN, EW_GREEN_FLASH, EW_YELLOW, EW_TO_NS_BUZZER,
  // Single-direction states
  RED, RED_FLASH, BUZZER, GREEN, GREEN_FLASH, YELLOW 
};
volatile State currentState = INITIAL;
volatile State nextState = NS_RED;          // Tracks next state after BUZZER
volatile unsigned long stateCounter = 0;    // Counts seconds in current state
volatile unsigned long flashCounter = 0;    // Counts milliseconds for flashing (incremented by 1000 each sec)
volatile bool flashState = false;           // LED flash state
volatile unsigned long buzzerCounter = 0;   // Counts milliseconds for buzzer (incremented by 1000 each sec)
volatile bool buzzerState = false;          // Buzzer toggle state
volatile byte remainingTime = 0;            // Remaining time for display (NS direction)

// Function to indicate dual mode activation
void indicateDualMode() {
  for (int i = 0; i < 3; i++) { // Three cycles of LED flash and buzzer beep
    // Turn all LEDs on
    PORTA |= (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7); // NS_RED, EW_RED, NS_YELLOW, EW_YELLOW
    PORTC |= (1 << 6) | (1 << 4); // NS_GREEN, EW_GREEN
    // Beep for 0.5s (~500 Hz)
    unsigned long beepStart = millis();
    while (millis() - beepStart < 500) {
      if (millis() - buzzerCounter >= 1) { // 1ms toggle for ~500 Hz
        buzzerState = !buzzerState;
        if (buzzerState) {
          PORTC |= (1 << 2); // Buzzer on
        } else {
          PORTC &= ~(1 << 2); // Buzzer off
        }
        buzzerCounter = millis();
      }
    }
    // Turn all LEDs and buzzer off
    PORTA &= ~((1 << 1) | (1 << 3) | (1 << 5) | (1 << 7));
    PORTC &= ~((1 << 6) | (1 << 4) | (1 << 2));
    // Wait 0.5s
    while (millis() - beepStart < 1000);
  }
}

// Set segments for a digit
void setSegments(byte digit) {
  byte pattern = digitPatterns[digit];
  for (int i = 0; i < 7; i++) {
    digitalWrite(segmentPins[i], (pattern & (1 << i)) ? HIGH : LOW);
  }
  digitalWrite(segmentPins[7], LOW); // dp off
}

// Timer1 interrupt service routine (called every 1s)
ISR(TIMER1_COMPA_vect) {
  if (!sequenceStarted) {
    // Blink Red LEDs until sequence starts
    flashCounter += 1000;
    if (flashCounter >= 1000) {
      flashState = !flashState;
      if (flashState) {
        PORTA |= (1 << 1) | (1 << 3); // NS_RED_PIN, EW_RED_PIN on
      } else {
        PORTA &= ~((1 << 1) | (1 << 3)); // NS_RED_PIN, EW_RED_PIN off
      }
      flashCounter = 0;
    }
    PORTC &= ~((1 << 6) | (1 << 4)); // NS_GREEN_PIN, EW_GREEN_PIN off
    PORTA &= ~((1 << 5) | (1 << 7)); // NS_YELLOW_PIN, EW_YELLOW_PIN off
    PORTC &= ~(1 << 2); // Buzzer off
    remainingTime = 0;
    return;
  }

  stateCounter++; 
  flashCounter += 1000;
  buzzerCounter += 1000; 

  if (dualMode) {
    // Dual-direction mode state machine
    switch (currentState) {
      case NS_RED:
        PORTA |= (1 << 3); // Force EW_RED_PIN on
        PORTA |= (1 << 1); // Set NS_RED_PIN on
        remainingTime = nsRedDuration - stateCounter;
        if (stateCounter >= nsRedDuration - 3) {
          currentState = NS_RED_FLASH;
          stateCounter = 0;
          flashCounter = 0;
          flashState = true;
        }
        break;

      case NS_RED_FLASH:
        PORTA |= (1 << 3); // Force EW_RED_PIN on
        if (flashCounter >= 500) {
          flashState = !flashState;
          if (flashState) {
            PORTA |= (1 << 1); // NS_RED_PIN on
          } else {
            PORTA &= ~(1 << 1); // NS_RED_PIN off
          }
          flashCounter = 0;
        }
        remainingTime = 3 - stateCounter;
        if (stateCounter >= 3) {
          currentState = NS_BUZZER;
          nextState = NS_GREEN;
          stateCounter = 0;
          buzzerCounter = 0;
          buzzerState = true;
          PORTA &= ~(1 << 1); // NS_RED_PIN off
          PORTC |= (1 << 2); // Buzzer on
        }
        break;

      case NS_BUZZER:
        PORTA |= (1 << 3); // Force EW_RED_PIN on
        if (buzzerCounter >= 1) { // 1ms toggle for ~500 Hz
          buzzerState = !buzzerState;
          if (buzzerState) {
            PORTC |= (1 << 2); // Buzzer on
          } else {
            PORTC &= ~(1 << 2); // Buzzer off
          }
          buzzerCounter = 0;
        }
        remainingTime = 0;
        if (stateCounter >= 3) {
          currentState = nextState;
          stateCounter = 0;
          PORTC &= ~(1 << 2); // Buzzer off
          if (nextState == NS_GREEN) {
            PORTC |= (1 << 6); // NS_GREEN_PIN on
          } else if (nextState == NS_YELLOW) {
            PORTA |= (1 << 5); // NS_YELLOW_PIN on
          }
        }
        break;

      case NS_GREEN:
        PORTA |= (1 << 3); // Force EW_RED_PIN on
        remainingTime = nsGreenDuration - stateCounter;
        if (stateCounter >= nsGreenDuration - 3) {
          currentState = NS_GREEN_FLASH;
          stateCounter = 0;
          flashCounter = 0;
          flashState = true;
          PORTC |= (1 << 6); // Ensure NS_GREEN_PIN on
        }
        break;

      case NS_GREEN_FLASH:
        PORTA |= (1 << 3); // Force EW_RED_PIN on
        if (flashCounter >= 500) {
          flashState = !flashState;
          if (flashState) {
            PORTC |= (1 << 6); // NS_GREEN_PIN on
          } else {
            PORTC &= ~(1 << 6); // NS_GREEN_PIN off
          }
          flashCounter = 0;
        }
        remainingTime = 3 - stateCounter;
        if (stateCounter >= 3) {
          currentState = NS_BUZZER;
          nextState = NS_YELLOW;
          stateCounter = 0;
          buzzerCounter = 0;
          buzzerState = true;
          PORTC &= ~(1 << 6); // NS_GREEN_PIN off
          PORTC |= (1 << 2); // Buzzer on
        }
        break;

      case NS_YELLOW:
        PORTA |= (1 << 3); // Force EW_RED_PIN on
        remainingTime = 3 - stateCounter;
        if (stateCounter >= 3) {
          currentState = NS_TO_EW_BUZZER;
          nextState = EW_RED;
          stateCounter = 0;
          buzzerCounter = 0;
          buzzerState = true;
          PORTA &= ~(1 << 5); // NS_YELLOW_PIN off
          PORTC |= (1 << 2); // Buzzer on
        }
        break;

      case NS_TO_EW_BUZZER:
        PORTA |= (1 << 1) | (1 << 3); // Force both NS_RED_PIN and EW_RED_PIN on
        if (buzzerCounter >= 1) {
          buzzerState = !buzzerState;
          if (buzzerState) {
            PORTC |= (1 << 2); // Buzzer on
          } else {
            PORTC &= ~(1 << 2); // Buzzer off
          }
          buzzerCounter = 0;
        }
        remainingTime = 0;
        if (stateCounter >= 3) {
          currentState = nextState;
          stateCounter = 0;
          PORTC &= ~(1 << 2); // Buzzer off
          PORTA |= (1 << 3); // EW_RED_PIN on
        }
        break;

      case EW_RED:
        PORTA |= (1 << 1); // Force NS_RED_PIN on
        PORTA |= (1 << 3); // Set EW_RED_PIN on
        remainingTime = 0;
        if (stateCounter >= ewRedDuration - 3) {
          currentState = EW_RED_FLASH;
          stateCounter = 0;
          flashCounter = 0;
          flashState = true;
        }
        break;

      case EW_RED_FLASH:
        PORTA |= (1 << 1); // Force NS_RED_PIN on
        if (flashCounter >= 500) {
          flashState = !flashState;
          if (flashState) {
            PORTA |= (1 << 3); // EW_RED_PIN on
          } else {
            PORTA &= ~(1 << 3); // EW_RED_PIN off
          }
          flashCounter = 0;
        }
        remainingTime = 0;
        if (stateCounter >= 3) {
          currentState = EW_BUZZER;
          nextState = EW_GREEN;
          stateCounter = 0;
          buzzerCounter = 0;
          buzzerState = true;
          PORTA &= ~(1 << 3); // EW_RED_PIN off
          PORTC |= (1 << 2); // Buzzer on
        }
        break;

      case EW_BUZZER:
        PORTA |= (1 << 1); // Force NS_RED_PIN on
        if (buzzerCounter >= 1) {
          buzzerState = !buzzerState;
          if (buzzerState) {
            PORTC |= (1 << 2); // Buzzer on
          } else {
            PORTC &= ~(1 << 2); // Buzzer off
          }
          buzzerCounter = 0;
        }
        remainingTime = 0;
        if (stateCounter >= 3) {
          currentState = nextState;
          stateCounter = 0;
          PORTC &= ~(1 << 2); // Buzzer off
          if (nextState == EW_GREEN) {
            PORTC |= (1 << 4); // EW_GREEN_PIN on
          } else if (nextState == EW_YELLOW) {
            PORTA |= (1 << 7); // EW_YELLOW_PIN on
          }
        }
        break;

      case EW_GREEN:
        PORTA |= (1 << 1); // Force NS_RED_PIN on
        remainingTime = 0;
        if (stateCounter >= ewGreenDuration - 3) {
          currentState = EW_GREEN_FLASH;
          stateCounter = 0;
          flashCounter = 0;
          flashState = true;
          PORTC |= (1 << 4); // Ensure EW_GREEN_PIN on
        }
        break;

      case EW_GREEN_FLASH:
        PORTA |= (1 << 1); // Force NS_RED_PIN on
        if (flashCounter >= 500) {
          flashState = !flashState;
          if (flashState) {
            PORTC |= (1 << 4); // EW_GREEN_PIN on
          } else {
            PORTC &= ~(1 << 4); // EW_GREEN_PIN off
          }
          flashCounter = 0;
        }
        remainingTime = 0;
        if (stateCounter >= 3) {
          currentState = EW_BUZZER;
          nextState = EW_YELLOW;
          stateCounter = 0;
          buzzerCounter = 0;
          buzzerState = true;
          PORTC &= ~(1 << 4); // EW_GREEN_PIN off
          PORTC |= (1 << 2); // Buzzer on
        }
        break;

      case EW_YELLOW:
        PORTA |= (1 << 1); // Force NS_RED_PIN on
        remainingTime = 0;
        if (stateCounter >= 3) {
          currentState = EW_TO_NS_BUZZER;
          nextState = NS_RED;
          stateCounter = 0;
          buzzerCounter = 0;
          buzzerState = true;
          PORTA &= ~(1 << 7); // EW_YELLOW_PIN off
          PORTC |= (1 << 2); // Buzzer on
        }
        break;

      case EW_TO_NS_BUZZER:
        PORTA |= (1 << 1) | (1 << 3); // Force both NS_RED_PIN and EW_RED_PIN on
        if (buzzerCounter >= 1) {
          buzzerState = !buzzerState;
          if (buzzerState) {
            PORTC |= (1 << 2); // Buzzer on
          } else {
            PORTC &= ~(1 << 2); // Buzzer off
          }
          buzzerCounter = 0;
        }
        remainingTime = 0;
        if (stateCounter >= 3) {
          currentState = nextState;
          stateCounter = 0;
          PORTC &= ~(1 << 2); // Buzzer off
          PORTA |= (1 << 1); // NS_RED_PIN on
        }
        break;
    }
    // sync for Green and Yellow states
    if (currentState == NS_GREEN || currentState == NS_GREEN_FLASH || currentState == NS_YELLOW) {
      PORTA |= (1 << 3); // EW_RED_PIN on
    } else if (currentState == EW_GREEN || currentState == EW_GREEN_FLASH || currentState == EW_YELLOW) {
      PORTA |= (1 << 1); // NS_RED_PIN on
    }
  } else {
    // Single-direction mode state machine
    switch (currentState) {
      case RED:
        if (stateCounter >= nsRedDuration - 3) {
          currentState = RED_FLASH;
          stateCounter = 0;
          flashCounter = 0;
          flashState = true;
          PORTA |= (1 << 1) | (1 << 3); // NS_RED_PIN, EW_RED_PIN on
        }
        break;

      case RED_FLASH:
        if (flashCounter >= 500) {
          flashState = !flashState;
          if (flashState) {
            PORTA |= (1 << 1) | (1 << 3); // NS_RED_PIN, EW_RED_PIN on
          } else {
            PORTA &= ~((1 << 1) | (1 << 3)); // NS_RED_PIN, EW_RED_PIN off
          }
          flashCounter = 0;
        }
        if (stateCounter >= 3) {
          currentState = BUZZER;
          nextState = GREEN;
          stateCounter = 0;
          buzzerCounter = 0;
          buzzerState = true;
          PORTA &= ~((1 << 1) | (1 << 3)); // NS_RED_PIN, EW_RED_PIN off
          PORTC |= (1 << 2); // Buzzer on
        }
        break;

      case BUZZER:
        if (buzzerCounter >= 1) {
          buzzerState = !buzzerState;
          if (buzzerState) {
            PORTC |= (1 << 2); // Buzzer on
          } else {
            PORTC &= ~(1 << 2); // Buzzer off
          }
          buzzerCounter = 0;
        }
        if (stateCounter >= 3) {
          currentState = nextState;
          stateCounter = 0;
          PORTC &= ~(1 << 2); // Buzzer off
          if (nextState == GREEN) {
            PORTC |= (1 << 6) | (1 << 4); // NS_GREEN_PIN, EW_GREEN_PIN on
          } else if (nextState == YELLOW) {
            PORTA |= (1 << 5) | (1 << 7); // NS_YELLOW_PIN, EW_YELLOW_PIN on
          } else if (nextState == RED) {
            PORTA |= (1 << 1) | (1 << 3); // NS_RED_PIN, EW_RED_PIN on
          }
        }
        break;

      case GREEN:
        if (stateCounter >= nsGreenDuration - 3) {
          currentState = GREEN_FLASH;
          stateCounter = 0;
          flashCounter = 0;
          flashState = true;
          PORTC |= (1 << 6) | (1 << 4); // NS_GREEN_PIN, EW_GREEN_PIN on
        }
        break;

      case GREEN_FLASH:
        if (flashCounter >= 500) {
          flashState = !flashState;
          if (flashState) {
            PORTC |= (1 << 6) | (1 << 4); // NS_GREEN_PIN, EW_GREEN_PIN on
          } else {
            PORTC &= ~((1 << 6) | (1 << 4)); // NS_GREEN_PIN, EW_GREEN_PIN off
          }
          flashCounter = 0;
        }
        if (stateCounter >= 3) {
          currentState = BUZZER;
          nextState = YELLOW;
          stateCounter = 0;
          buzzerCounter = 0;
          buzzerState = true;
          PORTC &= ~((1 << 6) | (1 << 4)); // NS_GREEN_PIN, EW_GREEN_PIN off
          PORTC |= (1 << 2); // Buzzer on
        }
        break;

      case YELLOW:
        if (stateCounter >= 3) {
          currentState = BUZZER;
          nextState = RED;
          stateCounter = 0;
          buzzerCounter = 0;
          buzzerState = true;
          PORTA &= ~((1 << 5) | (1 << 7)); // NS_YELLOW_PIN, EW_YELLOW_PIN off
          PORTC |= (1 << 2); // Buzzer on
        }
        break;
    }
  }
}

void setup() {
  start(); // Initialize pins using assembly
  Serial.begin(9600);
  keypad.setDebounceTime(50);

  // Initialize 7-segment display pins
  for (int i = 0; i < 8; i++) {
    pinMode(segmentPins[i], OUTPUT);
  }
  for (int i = 0; i < numDigits; i++) {
    pinMode(digitPins[i], OUTPUT);
    digitalWrite(digitPins[i], HIGH); 
  }
  // Interrupt code from profs github
  cli(); 
  TCCR1A = 0; 
  TCCR1B = 0;
  TCNT1 = 0; 
  OCR1A = 15624; 
  TCCR1B |= (1 << WGM12); 
  TCCR1B |= (1 << CS12) | (1 << CS10); 
  TIMSK1 |= (1 << OCIE1A); 
  sei(); 
}

void loop() {
  // Handle keypad input
  char key = keypad.getKey();
  if (key) {
    Serial.print(key);
    cli(); // Disable interrupts for state variable updates
    if (key == '*' && durationsSet && !sequenceStarted) {
      sequenceStarted = true;
      currentState = dualMode ? NS_RED : RED;
      stateCounter = 0;
      flashCounter = 0;
      buzzerCounter = 0;
      remainingTime = dualMode ? nsRedDuration : 0; // Initial remaining time only for dual mode
      // Initialize LEDs
      PORTA |= (1 << 1) | (1 << 3); // NS_RED_PIN, EW_RED_PIN on
      PORTC &= ~((1 << 6) | (1 << 4)); // NS_GREEN_PIN, EW_GREEN_PIN off
      PORTA &= ~((1 << 5) | (1 << 7)); // NS_YELLOW_PIN, EW_YELLOW_PIN off
      PORTC &= ~(1 << 2); // Buzzer off
    } else if ((key == '5' && inputIndex == 0) || 
               (key == '5' && inputIndex == 1 && inputBuffer[0] == '5') ||
               (key == '2' && inputIndex == 2 && inputBuffer[0] == '5' && inputBuffer[1] == '5') ||
               (key == '0' && inputIndex == 3 && inputBuffer[0] == '5' && inputBuffer[1] == '5' && inputBuffer[2] == '2')) {
      inputBuffer[inputIndex++] = key; // Build 5520 sequence
    } else if (key == '#' && inputIndex == 4 && inputBuffer[0] == '5' && inputBuffer[1] == '5' && inputBuffer[2] == '2' && inputBuffer[3] == '0') {
      dualMode = true; // activate dual-direction mode
      // Reset system state
      nsRedDuration = 0;
      nsGreenDuration = 0;
      ewRedDuration = 0;
      ewGreenDuration = 0;
      durationsSet = false;
      sequenceStarted = false;
      inputIndex = 0;
      currentState = INITIAL;
      stateCounter = 0;
      flashCounter = 0;
      buzzerCounter = 0;
      remainingTime = 0;
      sei(); // Re-enable interrupts
      indicateDualMode(); 
      cli(); // Disable interrupts again
    } else if (key == 'A' || key == 'B' || (dualMode && (key == 'C' || key == 'D'))) {
      inputBuffer[0] = key; // Start new sequence (A, B, C, D)
      inputIndex = 1;
    } else if (inputIndex > 0 && inputIndex < 3 && (key >= '0' && key <= '9')) {
      inputBuffer[inputIndex++] = key; // Store digit
    } else if (key == '#' && inputIndex >= 2) {
      // Parse input sequence
      if (inputBuffer[0] == 'A') {
        nsRedDuration = (inputBuffer[1] - '0') * 10 + (inputBuffer[2] - '0');
      } else if (inputBuffer[0] == 'B') {
        nsGreenDuration = (inputBuffer[1] - '0') * 10 + (inputBuffer[2] - '0');
      } else if (dualMode && inputBuffer[0] == 'C') {
        ewRedDuration = (inputBuffer[1] - '0') * 10 + (inputBuffer[2] - '0');
      } else if (dualMode && inputBuffer[0] == 'D') {
        ewGreenDuration = (inputBuffer[1] - '0') * 10 + (inputBuffer[2] - '0');
      }
      inputIndex = 0; // Reset buffer
      durationsSet = dualMode ? (nsRedDuration > 0 && nsGreenDuration > 0 && ewRedDuration > 0 && ewGreenDuration > 0)
                              : (nsRedDuration > 0 && nsGreenDuration > 0);
    }
    sei(); // Re-enable interrupts
  }

  // 7-segment display only in dual mode
  if (dualMode) {
    static unsigned long lastRefresh = 0;
    static byte currDigit = 0;
    unsigned long now = millis();
    if (now - lastRefresh >= 5) {
      digitalWrite(digitPins[0], HIGH);
      digitalWrite(digitPins[1], HIGH);
      byte tens = remainingTime / 10;
      byte units = remainingTime % 10;
      if (currDigit == 0) {
        setSegments(tens);
        digitalWrite(digitPins[0], LOW); // Activate tens digit
      } else {
        setSegments(units);
        digitalWrite(digitPins[1], LOW); // Activate units digit
      }
      currDigit = 1 - currDigit;
      lastRefresh = now;
    }
  } else {
    // Turn off display in single mode
    digitalWrite(digitPins[0], HIGH);
    digitalWrite(digitPins[1], HIGH);
  }
}