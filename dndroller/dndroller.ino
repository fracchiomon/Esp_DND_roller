// Types must be defined before Arduino's auto-generated prototypes
#include "config.h"
#include "types.h"
#include "geometry.h"
#include "karma.h"
#include "dice_3d.h"
#include "ui_buttons.h"
#include "ui_display.h"
#include "led.h"
#include "serial_cmd.h"
#include "touch.h"

#include <TFT_eSPI.h>
#include <TFT_eWidget.h>
#include <LittleFS.h>
using namespace fs;
#include <stdint.h>
#include "esp_system.h"
#include "esp_sleep.h"

// Global TFT instance
TFT_eSPI tft = TFT_eSPI();

// Game state
unsigned long lastActivityTime = 0;

// State variables for loop touch handling
static uint32_t lastScan        = 0;
static uint32_t lastPressTime   = 0;
static bool     repeatTriggered = false;

// Forward declarations for loop management
extern ButtonWidget* diceButtons[7];
extern ButtonWidget* quantityUpBtn;
extern ButtonWidget* quantityDownBtn;
extern ButtonWidget* rollBtn;
extern ButtonWidget* rngModeBtn;
extern ButtonWidget* advBtn;
extern int selectedDiceIndex;
extern bool animationActive, isRolling;
extern unsigned long animationStartTime;
extern bool resultsShown;
extern void animateDice();
extern void displayResults();

void setup() {
  Serial.begin(115200);
  if (DEBUG_MODE) {
    Serial.println("=== DnD Roller DEBUG MODE ===");
    Serial.println("Serial Plotter: Roll / KarmaAvg");
  }
  
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(1);

  // Initialize geometries
  initD10Geometry();
  
  // Build D10 faces
  d10TriCount = 0;
  for (int i = 0; i < 5; i++) {
    int ui  = 2 + i;
    int ui1 = 2 + ((i + 1) % 5);
    int li  = 7 + i;
    d10TriFaces[d10TriCount][0] = 0; d10TriFaces[d10TriCount][1] = ui;  d10TriFaces[d10TriCount++][2] = li;
    d10TriFaces[d10TriCount][0] = 0; d10TriFaces[d10TriCount][1] = li;  d10TriFaces[d10TriCount++][2] = ui1;
    d10TriFaces[d10TriCount][0] = 1; d10TriFaces[d10TriCount][1] = li;  d10TriFaces[d10TriCount++][2] = ui;
    d10TriFaces[d10TriCount][0] = 1; d10TriFaces[d10TriCount][1] = ui;  d10TriFaces[d10TriCount++][2] = ((7 + ((i + 4) % 5)));
  }

  // Dodeca triangulation
  const int pentagons[12][5] = {
    {0,8,10,4,16}, {1,8,10,5,17}, {2,9,11,6,18}, {3,9,11,7,19},
    {0,12,13,1,8}, {2,12,13,3,9}, {4,14,15,5,10}, {6,14,15,7,11},
    {0,16,18,2,12}, {4,16,18,6,14}, {1,17,19,3,13}, {5,17,19,7,15}
  };
  dodecaTriCount = 0;
  for (int p = 0; p < 12; p++) {
    int v0 = pentagons[p][0];
    for (int i = 1; i < 4; i++) {
      int v1 = pentagons[p][i];
      int v2 = pentagons[p][i+1];
      dodecaTriFaces[dodecaTriCount][0] = v0;
      dodecaTriFaces[dodecaTriCount][1] = v1;
      dodecaTriFaces[dodecaTriCount++][2] = v2;
    }
    int v1 = pentagons[p][1], v2 = pentagons[p][2], v3 = pentagons[p][3], v4 = pentagons[p][4];
    dodecaTriFaces[dodecaTriCount][0] = v1; dodecaTriFaces[dodecaTriCount][1] = v2; dodecaTriFaces[dodecaTriCount++][2] = v3;
    dodecaTriFaces[dodecaTriCount][0] = v1; dodecaTriFaces[dodecaTriCount][1] = v3; dodecaTriFaces[dodecaTriCount++][2] = v4;
  }

  // Build icosa faces
  buildIcosaFaces();

  touch_calibrate();

  // LED setup
  ledcAttach(LED_RED_PIN,   5000, 8);
  ledcAttach(LED_GREEN_PIN, 5000, 8);
  ledcAttach(LED_BLUE_PIN,  5000, 8);
  ledApplyState();

  lastActivityTime = millis();
  setupDiceButtons();
}

void loop() {
  // Serial command input
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    handleSerialCommand(cmd);
  }

  // Run dice animation
  if (selectedDiceIndex != -1) {
    animateDice();
    
    if (isRolling && (millis() - animationStartTime > 1000)) {
      isRolling = false;
    }
    
    if (!resultsShown && animationActive && (millis() - animationStartTime > 1500)) {
      animationActive = false;
      resultsShown = true;
      displayResults();
    }
  }
  
  // Touch handling
  if (millis() - lastScan >= 50) {
    uint16_t x, y;
    bool touched = tft.getTouch(&x, &y);

    ButtonWidget* allButtons[] = {
      diceButtons[0], diceButtons[1], diceButtons[2], diceButtons[3],
      diceButtons[4], diceButtons[5], diceButtons[6],
      quantityUpBtn, quantityDownBtn, rollBtn, rngModeBtn, advBtn
    };
    int totalButtons = 12;

    if (touched) {
      lastActivityTime = millis();
      for (int i = 0; i < totalButtons; i++) {
        if (allButtons[i]->contains(x, y)) {
          if (!allButtons[i]->isPressed()) {
            allButtons[i]->press(true);
            allButtons[i]->pressAction();
            lastPressTime   = millis();
            repeatTriggered = false;
          } else {
            if (allButtons[i] == quantityUpBtn || allButtons[i] == quantityDownBtn) {
              uint32_t held = millis() - lastPressTime;
              uint32_t threshold = repeatTriggered ? 200UL : 600UL;
              if (held >= threshold) {
                allButtons[i]->pressAction();
                lastPressTime   = millis();
                repeatTriggered = true;
              }
            }
          }
          break;
        }
      }
    } else {
      for (int i = 0; i < totalButtons; i++) allButtons[i]->press(false);
      repeatTriggered = false;
    }

    // Auto-sleep
    if (millis() - lastActivityTime > SLEEP_TIMEOUT) {
      tft.fillScreen(TFT_BLACK);
      tft.setTextDatum(MC_DATUM);
      tft.setTextSize(2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString("Buona notte...", 160, 120);
      delay(1000);
      tft.fillScreen(TFT_BLACK);
      esp_sleep_enable_ext0_wakeup(GPIO_NUM_36, 0);
      esp_deep_sleep_start();
    }

    lastScan = millis();
  }
}