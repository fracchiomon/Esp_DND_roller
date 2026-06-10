#include "ui_buttons.h"
#include "ui_display.h"
#include "led.h"
#include "config.h"
#include "types.h"
#include <TFT_eWidget.h>

extern TFT_eSPI tft;

int selectedDiceIndex = -1;
int diceQuantity = 1;
AdvState advState = ADV_NORMAL;
bool useKarmicDice = false;

ButtonWidget* diceButtons[7];
ButtonWidget* quantityUpBtn;
ButtonWidget* quantityDownBtn;
ButtonWidget* rollBtn;
ButtonWidget* advBtn;
ButtonWidget* rngModeBtn;

char diceLabels[7][6] = {"D4", "D6", "D8", "D10", "D12", "D20", "D100"};
int diceSides[7] = {4, 6, 8, 10, 12, 20, 100};
ButtonPos diceButtonPos[7];
uint8_t buttonCount = 7;

extern void drawCenteredLabel(int x, int y, int w, int h, const char* label, uint8_t textSize, uint16_t color, uint16_t bg);
extern void rollDice();
extern void updateKarmaState();

void btn0_action() { selectedDiceIndex = 0; updateDiceSelection(); advState = ADV_NORMAL; updateAdvButton(); ledApplyState(); }
void btn1_action() { selectedDiceIndex = 1; updateDiceSelection(); advState = ADV_NORMAL; updateAdvButton(); ledApplyState(); }
void btn2_action() { selectedDiceIndex = 2; updateDiceSelection(); advState = ADV_NORMAL; updateAdvButton(); ledApplyState(); }
void btn3_action() { selectedDiceIndex = 3; updateDiceSelection(); advState = ADV_NORMAL; updateAdvButton(); ledApplyState(); }
void btn4_action() { selectedDiceIndex = 4; updateDiceSelection(); advState = ADV_NORMAL; updateAdvButton(); ledApplyState(); }
void btn5_action() { selectedDiceIndex = 5; updateDiceSelection(); advState = ADV_NORMAL; updateAdvButton(); ledApplyState(); }
void btn6_action() { selectedDiceIndex = 6; updateDiceSelection(); advState = ADV_NORMAL; updateAdvButton(); ledApplyState(); }

void (*btnActions[])() = {
  btn0_action, btn1_action, btn2_action, btn3_action,
  btn4_action, btn5_action, btn6_action
};

void quantityUp_action() {
  if (diceQuantity < MAX_ROLLS_AVAILABLE) {
    diceQuantity++;
    updateQuantityDisplay();
  }
}

void quantityDown_action() {
  if (diceQuantity > 1) {
    diceQuantity--;
    updateQuantityDisplay();
  }
}

void roll_action() {
  rollDice();
}

void updateQuantityDisplay() {
  const int qx=250, qy=180, qw=20, qh=35;
  tft.fillRect(qx, qy, qw, qh, TFT_BLACK);
  uint8_t ts = (diceQuantity < 10) ? 2 : 1;
  tft.setTextSize(ts);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawNumber(diceQuantity, qx + qw / 2, qy + qh / 2);
  tft.setTextDatum(TL_DATUM);
}

void updateDiceSelection() {
  for (int i = 0; i < buttonCount; i++) {
    uint16_t fill = (i == selectedDiceIndex) ? TFT_GREEN : TFT_BLUE;
    diceButtons[i]->initButtonUL(diceButtonPos[i].x, diceButtonPos[i].y, 
                                 diceButtonPos[i].w, diceButtonPos[i].h,
                                 TFT_WHITE, fill, TFT_WHITE, "", 2);
    diceButtons[i]->drawSmoothButton(false, 3, TFT_BLACK);
    drawCenteredLabel(diceButtonPos[i].x, diceButtonPos[i].y,
                      diceButtonPos[i].w, diceButtonPos[i].h,
                      diceLabels[i], 2, TFT_WHITE, fill);
  }
}

void updateAdvButton() {
  uint16_t fill; uint16_t tcol; const char* lbl;
  if (selectedDiceIndex != 5) {
    fill = 0x4208; tcol = 0x8410; lbl = "ADV  solo D20";
  } else {
    switch (advState) {
      case ADV_NORMAL:
        fill = tft.color565(180,130,0); tcol = TFT_WHITE; lbl = "D20: NORMALE"; break;
      case ADV_VANTAGGIO:
        fill = TFT_GREEN;  tcol = TFT_BLACK; lbl = "D20: VANTAGGIO"; break;
      case ADV_SVANTAGGIO:
        fill = TFT_RED;    tcol = TFT_WHITE; lbl = "D20: SVANTAGGIO"; break;
      default:
        fill = 0x4208; tcol = TFT_WHITE; lbl = ""; break;
    }
  }
  advBtn->initButtonUL(125, 167, 195, 11, TFT_BLACK, fill, tcol, "", 1);
  advBtn->drawSmoothButton(false, 1, TFT_BLACK);
  drawCenteredLabel(125, 167, 195, 11, lbl, 1, tcol, fill);
}

void advMode_action() {
  if (selectedDiceIndex != 5) return;
  advState = (AdvState)((advState + 1) % 3);
  updateAdvButton();
  ledApplyState();
  if (DEBUG_MODE)
    Serial.printf("[ADV] stato -> %s\n",
      advState==ADV_NORMAL?"NORMALE": advState==ADV_VANTAGGIO?"VANTAGGIO":"SVANTAGGIO");
}

void rngMode_action() {
  useKarmicDice = !useKarmicDice;
  updateKarmaState();
  
  uint16_t fill  = useKarmicDice ? TFT_GREEN  : (uint16_t)0x7BEF;
  uint16_t tcol  = useKarmicDice ? TFT_BLACK  : TFT_WHITE;
  const char* lbl = useKarmicDice ? "KARMA ON" : "RNG PURO";
  rngModeBtn->initButtonUL(125, 218, 195, 20, TFT_BLACK, fill, tcol, "", 1);
  rngModeBtn->drawSmoothButton(false, 1, TFT_BLACK);
  drawCenteredLabel(125, 218, 195, 20, lbl, 1, tcol, fill);
}

void setupDiceButtons() {
  int btnWidth = 110;
  int btnHeight = 30;
  int spacing = 3;
  int leftX = 5;
  int startY = 5;

  for (int i = 0; i < buttonCount; i++) {
    int x = leftX;
    int y = startY + i * (btnHeight + spacing);

    diceButtonPos[i] = {x, y, btnWidth, btnHeight};

    diceButtons[i] = new ButtonWidget(&tft);
    diceButtons[i]->initButtonUL(x, y, btnWidth, btnHeight, TFT_WHITE, TFT_BLUE, TFT_WHITE, "", 2);
    diceButtons[i]->setPressAction(btnActions[i]);
    diceButtons[i]->drawSmoothButton(false, 3, TFT_BLACK);
    drawCenteredLabel(x, y, btnWidth, btnHeight, diceLabels[i], 2, TFT_WHITE, TFT_BLUE);
  }
  
  int rollX = 125;
  int rollY = 180;
  rollBtn = new ButtonWidget(&tft);
  rollBtn->initButtonUL(rollX, rollY, 90, 35, TFT_WHITE, TFT_RED, TFT_WHITE, "", 3);
  rollBtn->setPressAction(roll_action);
  rollBtn->drawSmoothButton(false, 3, TFT_BLACK);
  drawCenteredLabel(rollX, rollY, 90, 35, "ROLL", 3, TFT_WHITE, TFT_RED);
  
  int qtyY = 180;
  quantityDownBtn = new ButtonWidget(&tft);
  quantityDownBtn->initButtonUL(220, qtyY, 30, 35, TFT_WHITE, TFT_CYAN, TFT_BLACK, "", 3);
  quantityDownBtn->setPressAction(quantityDown_action);
  quantityDownBtn->drawSmoothButton(false, 2, TFT_BLACK);
  drawCenteredLabel(220, qtyY, 30, 35, "-", 3, TFT_BLACK, TFT_CYAN);
  
  quantityUpBtn = new ButtonWidget(&tft);
  quantityUpBtn->initButtonUL(270, qtyY, 30, 35, TFT_WHITE, TFT_CYAN, TFT_BLACK, "", 3);
  quantityUpBtn->setPressAction(quantityUp_action);
  quantityUpBtn->drawSmoothButton(false, 2, TFT_BLACK);
  drawCenteredLabel(270, qtyY, 30, 35, "+", 3, TFT_BLACK, TFT_CYAN);
  
  updateQuantityDisplay();

  advBtn = new ButtonWidget(&tft);
  advBtn->initButtonUL(125, 167, 195, 11, TFT_BLACK, (uint16_t)0x4208, (uint16_t)0x8410, "", 1);
  advBtn->setPressAction(advMode_action);
  advBtn->drawSmoothButton(false, 1, TFT_BLACK);
  drawCenteredLabel(125, 167, 195, 11, "ADV  solo D20", 1, (uint16_t)0x8410, (uint16_t)0x4208);

  rngModeBtn = new ButtonWidget(&tft);
  rngModeBtn->initButtonUL(125, 218, 195, 20, TFT_BLACK, (uint16_t)0x7BEF, TFT_WHITE, "", 1);
  rngModeBtn->setPressAction(rngMode_action);
  rngModeBtn->drawSmoothButton(false, 1, TFT_BLACK);
  drawCenteredLabel(125, 218, 195, 20, "RNG PURO", 1, TFT_WHITE, (uint16_t)0x7BEF);
}