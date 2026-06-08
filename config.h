// ============================================================================
// CONFIG.H - Global Configuration & Constants
// ============================================================================

#ifndef CONFIG_H
#define CONFIG_H

// ── RGB LED (active-LOW on CYD) ──────────────────────────────────────────
#define LED_RED_PIN    4
#define LED_GREEN_PIN  16
#define LED_BLUE_PIN   17
#define LED_DUTY_OFF   255   // active-LOW: 255 = off
#define LED_DUTY_ON    204   // 80% duty = ~20% brightness

// ── TOUCH CALIBRATION ────────────────────────────────────────────────────
#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL false

// ── ANIMATION TIMING ────────────────────────────────────────────────────
const unsigned long ROLLING_ANIMATION_DURATION = 2000UL;  // 2 seconds
const unsigned long TOTAL_ANIMATION_DURATION = 3000UL;    // 3 seconds
const unsigned long ANIMATION_FRAME_MS = 50UL;            // 20 FPS

// ── POWER MANAGEMENT ────────────────────────────────────────────────────
const unsigned long SLEEP_TIMEOUT = 3UL * 60UL * 1000UL;  // 3 minutes

// ── 3D RENDERING ────────────────────────────────────────────────────────
const float ORTHO_SCALE = 40.0f;         // Global scale for all dice
const float D4_SCALE_FACTOR = 0.60f;     // D4-specific shrink factor

// ── DICE CONFIGURATION ──────────────────────────────────────────────────
#define NUM_DICE_TYPES 7
#define MAX_DICE_QUANTITY 10
#define MAX_DICE_RESULTS 10

#endif // CONFIG_H
