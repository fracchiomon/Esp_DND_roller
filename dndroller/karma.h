#ifndef KARMA_H
#define KARMA_H

// ── RNG Functions ──────────────────────────────────────────────────
int uniformIntInclusive(int minInclusive, int maxInclusive);
int rollUnbiasedDie(int sides);

// ── Karmic System ──────────────────────────────────────────────────
void updateKarma(float normalizedRoll);
float getKarmaAverage();
int rollDie(int sides);

// ── State (extern for access from main) ──────────────────────────
extern bool useKarmicDice;
extern float karmaHistory[10];
extern int karmaCount;

#endif