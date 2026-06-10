#include "ui_display.h"
#include "ui_buttons.h"
#include "config.h"
#include "karma.h"
#include "dice_3d.h"
#include <TFT_eSPI.h>

extern TFT_eSPI tft;

int diceRolledSinceStart = 0;
int rollResults[MAX_ROLLS_AVAILABLE];
int totalResult = 0;
int advRoll1 = 0, advRoll2 = 0;
bool resultsShown = false;

extern int selectedDiceIndex;
extern int diceQuantity;
extern AdvState advState;
extern int diceSides[7];
extern void updateKarmaState();
extern bool animationActive;
extern bool isRolling;
extern unsigned long animationStartTime;

void drawCenteredLabel(int x, int y, int w, int h, const char* label, uint8_t textSize, uint16_t color, uint16_t bg) {
  tft.setTextSize(textSize);
  tft.setTextColor(color, bg);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(label, x + w / 2, y + h / 2);
  tft.setTextDatum(TL_DATUM);
}

void displayResults() {
  tft.fillRect(125, 5, 190, 35, TFT_BLACK);
  Serial.printf("Dice rolled since start: %d\n", diceRolledSinceStart);
  if (selectedDiceIndex == -1) return;
  
  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  
  if (diceQuantity > 1) {
    tft.setCursor(130, 8);
    tft.print("Rolls: ");
    for (int i = 0; i < diceQuantity && i < 6; i++) {
      tft.printf("%d ", rollResults[i]);
    }
    if (diceQuantity > 4 && diceQuantity < 7) tft.setTextSize(1);
    if (diceQuantity > 6) {
      tft.print("...");
    }
    tft.setCursor(130, 24);
    tft.setTextSize(2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.printf("Total: %d", totalResult);
  } else {
    if (selectedDiceIndex == 5 && advState != ADV_NORMAL) {
      const char* advLabel = (advState == ADV_VANTAGGIO) ? "VANT." : "SVAN.";
      uint16_t c1 = (rollResults[0] == advRoll1) ? TFT_GREEN  : TFT_DARKGREY;
      uint16_t c2 = (rollResults[0] == advRoll2) ? TFT_GREEN  : TFT_DARKGREY;
      tft.setCursor(130, 5);
      tft.setTextSize(1);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.printf("%s  ", advLabel);
      tft.setTextColor(c1, TFT_BLACK);
      tft.printf("%d", advRoll1);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.print(" / ");
      tft.setTextColor(c2, TFT_BLACK);
      tft.printf("%d", advRoll2);
      tft.setCursor(130, 17);
      tft.setTextSize(2);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.printf("-> %d", rollResults[0]);
    } else {
      tft.setCursor(130, 8);
      tft.setTextSize(2);
      tft.print("Result: ");
      tft.setTextSize(3);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.printf("%d", rollResults[0]);
    }
  }
}

void rollDice() {
  if (selectedDiceIndex == -1) return;
  
  animationActive = true;
  isRolling = true;
  animationStartTime = millis();
  resultsShown = false;
  
  angleX = 0;
  angleY = 0;
  angleZ = 0;

  updateKarmaState();
  
  totalResult = 0;
  int sides = diceSides[selectedDiceIndex];
  bool useAdv = (selectedDiceIndex == 5 && advState != ADV_NORMAL);

  for (int i = 0; i < diceQuantity; i++) {
    if (useAdv) {
      int r1 = rollDie(sides);
      int r2 = rollDie(sides);
      advRoll1 = r1; advRoll2 = r2;
      rollResults[i] = (advState == ADV_VANTAGGIO) ? max(r1, r2) : min(r1, r2);
      Serial.printf("[ADV] dado1=%d  dado2=%d  usato=%d  (%s)\n",
        r1, r2, rollResults[i],
        (advState == ADV_VANTAGGIO) ? "VANTAGGIO" : "SVANTAGGIO");
      diceRolledSinceStart += 2;
    } else {
      rollResults[i] = rollDie(sides);
      diceRolledSinceStart++;
    }
    totalResult += rollResults[i];
  }
}