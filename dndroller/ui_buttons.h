#ifndef UI_BUTTONS_H
#define UI_BUTTONS_H

// ── Button Setup ───────────────────────────────────────────────────
void setupDiceButtons();

// ── Button Actions ────────────────────────────────────────────────
void btn0_action();
void btn1_action();
void btn2_action();
void btn3_action();
void btn4_action();
void btn5_action();
void btn6_action();
void quantityUp_action();
void quantityDown_action();
void roll_action();
void advMode_action();
void rngMode_action();

// ── UI Updates ────────────────────────────────────────────────────
void updateQuantityDisplay();
void updateDiceSelection();
void updateAdvButton();

// ── State (extern for access) ─────────────────────────────────────
extern int selectedDiceIndex;
extern int diceQuantity;

#endif