#ifndef CONFIG_H
#define CONFIG_H

// ── Debug ─────────────────────────────────────────────────────────────
#define DEBUG_MODE true

// ── RGB LED (active-LOW on CYD) ──────────────────────────────────────
#define LED_RED_PIN    4
#define LED_GREEN_PIN  16
#define LED_BLUE_PIN   17
#define LED_DUTY_OFF   255   // active-LOW: 255 = off
#define LED_DUTY_ON    204   // 80% duty = ~20% brightness

// ── Touch Calibration ────────────────────────────────────────────────
#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL false

// ── Game State ───────────────────────────────────────────────────────
#define MAX_ROLLS_AVAILABLE 20
#define KARMA_HISTORY       10
#define KARMA_STRENGTH      0.45f
#define KARMA_THRESHOLD     0.08f
#define SLEEP_TIMEOUT       (3UL * 60UL * 1000UL)

// ── Graphics / Projection ────────────────────────────────────────────
#define ORTHO_SCALE         40.0f
#define D4_SCALE_FACTOR     0.60f

#endif