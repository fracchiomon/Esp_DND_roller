#include "led.h"
#include "config.h"
#include "ui_buttons.h"
#include <Arduino.h>

extern int selectedDiceIndex;
extern AdvState advState;

void ledApplyState() {
  if (selectedDiceIndex != 5) {
    ledcWrite(LED_RED_PIN,   LED_DUTY_OFF);
    ledcWrite(LED_GREEN_PIN, LED_DUTY_ON);
    ledcWrite(LED_BLUE_PIN,  LED_DUTY_ON);
  } else {
    switch (advState) {
      case ADV_NORMAL:
        ledcWrite(LED_RED_PIN,   0);
        ledcWrite(LED_GREEN_PIN, 0);
        ledcWrite(LED_BLUE_PIN,  255);
        break;
      case ADV_VANTAGGIO:
        ledcWrite(LED_RED_PIN,   255);
        ledcWrite(LED_GREEN_PIN, 0);
        ledcWrite(LED_BLUE_PIN,  255);
        break;
      case ADV_SVANTAGGIO:
        ledcWrite(LED_RED_PIN,   0);
        ledcWrite(LED_GREEN_PIN, 255);
        ledcWrite(LED_BLUE_PIN,  255);
        break;
    }
  }
}