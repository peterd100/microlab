#define __SFR_OFFSET 0x00
#include "avr/io.h"

; LED pins
#define LED_PIN_PA1 1
#define LED_PIN_PA3 3
#define LED_PIN_PA5 5
#define LED_PIN_PA7 7
#define LED_PIN_PC6 6
#define LED_PIN_PC4 4

; Buzzer pin
#define BUZZER_PIN_PC2 2

; Keypad row pins 
#define KEY_ROW_PA0 0
#define KEY_ROW_PA2 2
#define KEY_ROW_PA4 4
#define KEY_ROW_PA6 6

; Keypad column pins 
#define KEY_COL_PC7 7
#define KEY_COL_PC5 5
#define KEY_COL_PC3 3
#define KEY_COL_PC1 1

.global start
.section .text

start:
  ; Configure LED pins as outputs
  SBI DDRA, LED_PIN_PA1
  SBI DDRA, LED_PIN_PA3
  SBI DDRA, LED_PIN_PA5
  SBI DDRA, LED_PIN_PA7
  SBI DDRC, LED_PIN_PC6
  SBI DDRC, LED_PIN_PC4

  ; Configure buzzer pin as output
  SBI DDRC, BUZZER_PIN_PC2

  ; Configure keypad row pins as outputs
  SBI DDRA, KEY_ROW_PA0
  SBI DDRA, KEY_ROW_PA2
  SBI DDRA, KEY_ROW_PA4
  SBI DDRA, KEY_ROW_PA6

  ; Configure keypad column pins as inputs
  CBI DDRC, KEY_COL_PC7
  CBI DDRC, KEY_COL_PC5
  CBI DDRC, KEY_COL_PC3
  CBI DDRC, KEY_COL_PC1
  
  ; Enable pull-up resistors on keypad column pins
  SBI PORTC, KEY_COL_PC7
  SBI PORTC, KEY_COL_PC5
  SBI PORTC, KEY_COL_PC3
  SBI PORTC, KEY_COL_PC1
  RET