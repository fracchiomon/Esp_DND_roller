#ifndef UI_DISPLAY_H
#define UI_DISPLAY_H

// ── Result Display ─────────────────────────────────────────────────
void drawCenteredLabel(int x, int y, int w, int h, 
                      const char* label, uint8_t textSize, 
                      uint16_t color, uint16_t bg);
void displayResults();
void rollDice();

// ── State (extern for access) ─────────────────────────────────────
extern int rollResults[20];
extern int totalResult;
extern int advRoll1, advRoll2;
extern bool resultsShown;
extern int diceQuantity;
extern int diceRolledSinceStart;
extern int selectedDiceIndex;

#endif