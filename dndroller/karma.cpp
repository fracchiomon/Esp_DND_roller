#include "karma.h"
#include "config.h"
#include "esp_system.h"
#include <math.h>

#ifdef DEBUG_MODE
#include <Arduino.h>
#endif

bool useKarmicDice = false;
float karmaHistory[KARMA_HISTORY] = {0.5f};
int karmaCount = 0;

// ────────────────────────────────────────────────────────────────────
// Unbiased uniform integer generation using ESP32 hardware RNG
// ────────────────────────────────────────────────────────────────────

int uniformIntInclusive(int minInclusive, int maxInclusive) {
  if (maxInclusive <= minInclusive) return minInclusive;
  uint32_t span = (uint32_t)(maxInclusive - minInclusive + 1);
  uint32_t limit = UINT32_MAX - (UINT32_MAX % span);
  uint32_t r;
  do {
    r = esp_random();
  } while (r >= limit);
  return (int)(minInclusive + (r % span));
}

int rollUnbiasedDie(int sides) {
  if (sides <= 1) return 1;
  return uniformIntInclusive(1, sides);
}

// ────────────────────────────────────────────────────────────────────
// Karmic System Helpers
// ────────────────────────────────────────────────────────────────────

void updateKarma(float normalizedRoll) {
  karmaHistory[karmaCount % KARMA_HISTORY] = normalizedRoll;
  karmaCount++;
}

float getKarmaAverage() {
  int n = (karmaCount < KARMA_HISTORY) ? karmaCount : KARMA_HISTORY;
  if (n == 0) return 0.5f;
  float sum = 0.0f;
  for (int i = 0; i < n; i++) sum += karmaHistory[i];
  return sum / n;
}

// ────────────────────────────────────────────────────────────────────
// Roll one die respecting the current mode (pure RNG or karmic)
// ────────────────────────────────────────────────────────────────────

int rollDie(int sides) {
  if (sides <= 1) return 1;

  int roll = rollUnbiasedDie(sides);

  if (useKarmicDice) {
    float karma    = 0.5f - getKarmaAverage();
    float normRoll = (float)(roll - 1) / (float)(sides - 1);

    bool lowRoll  = normRoll < 0.4f;
    bool highRoll = normRoll > 0.6f;

    float rerollProb = (fabsf(karma) - KARMA_THRESHOLD) * KARMA_STRENGTH;
    if (rerollProb > 0.0f) {
      float r = (float)(esp_random() & 0xFFFF) / 65535.0f;
      bool doReroll = false;
      if (karma >  KARMA_THRESHOLD && lowRoll  && r < rerollProb) doReroll = true;
      if (karma < -KARMA_THRESHOLD && highRoll && r < rerollProb) doReroll = true;
      if (doReroll) roll = rollUnbiasedDie(sides);
    }

    updateKarma((float)(roll - 1) / (float)(sides - 1));

#ifdef DEBUG_MODE
    float kAvg = getKarmaAverage();
    float karma_val = 0.5f - kAvg;
    Serial.printf("[KARMA] roll=%d/%d  norm=%.2f  karmaAvg=%.3f  karmaVal=%.3f\n",
                  roll, sides,
                  (float)(roll-1)/(float)(sides-1),
                  kAvg, karma_val);
#endif
  }

#ifdef DEBUG_MODE
  if (useKarmicDice)
    Serial.printf("Roll:%d\tKarmaAvg:%.2f\n", roll, getKarmaAverage() * sides);
  else
    Serial.printf("Roll:%d\n", roll);
#endif

  return roll;
}