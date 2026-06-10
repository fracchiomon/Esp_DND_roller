// Types must be defined before Arduino's auto-generated prototypes
struct Point3D { float x, y, z; };
struct Point2D { int x, y; };

#include <TFT_eSPI.h>
#include <TFT_eWidget.h>
#include <LittleFS.h>
using namespace fs;
#include <stdint.h>
// Use ESP32 hardware RNG for unbiased dice rolls
#include "esp_system.h"
#include "esp_sleep.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <secrets.h>

// ── OTA Web Updater ───────────────────────────────────────────────────────────
// Change credentials before first flash; after that update via http://<ip>/update
#define OTA_PORT    80

WebServer otaServer(OTA_PORT);
bool      wifiConnected = false;
String    deviceIP      = "";
bool      otaInProgress = false;

// ── Debug ─────────────────────────────────────────────────────────────────────
// Set to true to enable Serial output (Serial Monitor + Serial Plotter)
#define DEBUG_MODE true

// ── RGB LED (active-LOW on CYD) ──────────────────────────────────────────────
#define LED_RED_PIN    4
#define LED_GREEN_PIN  16
#define LED_BLUE_PIN   17
#define LED_DUTY_OFF   255   // active-LOW: 255 = off
#define LED_DUTY_ON    204   // 80% duty = ~20% brightness

#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL false

TFT_eSPI tft = TFT_eSPI();
ButtonWidget* diceButtons[7];  // Increased to 7 for D100
ButtonWidget* quantityUpBtn;
ButtonWidget* quantityDownBtn;
ButtonWidget* rollBtn;

char diceLabels[7][6] = {"D4", "D6", "D8", "D10", "D12", "D20", "D100"};
int diceSides[7] = {4, 6, 8, 10, 12, 20, 100};
uint8_t buttonCount = 7;

// Game state variables
int diceRolledSinceStart = 0;
const uint8_t MAX_ROLLS_AVAILABLE = 20;
int rollResults[MAX_ROLLS_AVAILABLE];         // Store individual dice results
int selectedDiceIndex = -1;  // Track which dice is selected
int diceQuantity = 1;        // Number of dice to roll (1-10)
int totalResult = 0;         // Sum of all dice
int advRoll1 = 0, advRoll2 = 0;  // Both dice when adv/disadv active

// Button position storage for redrawing
struct ButtonPos {
  int x, y, w, h;
};
ButtonPos diceButtonPos[7];

// ── Karmic dice system ────────────────────────────────────────────────────────
// ── Advantage / Disadvantage (D20 only) ──────────────────────────────────────
enum AdvState { ADV_NORMAL, ADV_VANTAGGIO, ADV_SVANTAGGIO };
AdvState advState = ADV_NORMAL;
ButtonWidget* advBtn;

// ── BG3 Karmic dice system ────────────────────────────────────────────────────
// Source: https://bg3.wiki/wiki/Karmic_dice
//
// The system tracks a "debt" value. When a roll fails, p (probability of
// success) is added to debt. On subsequent rolls with debt > 0, a hidden roll
// determines if the die automatically succeeds with probability p/(1-debt).
// On auto-success, a value in [DC, sides] is chosen randomly, and
// 2*(1-p) is subtracted from debt.
//
// DC (Difficulty Class) must be known to compute p. Settable via serial: "dc N"
// Default: just above median of the die (e.g. DC 11 for D20 = 50% chance).

bool  useKarmicDice = false;
float karmicDebt    = 0.0f;   // accumulated debt (BG3 algorithm)
int   targetDC      = -1;     // -1 = auto (median+1), else user-set value

ButtonWidget* rngModeBtn;

// Returns the effective DC for a given die (auto mode = median+1)
int effectiveDC(int sides) {
  if (targetDC > 0 && targetDC <= sides) return targetDC;
  return sides / 2 + 1;  // just above median: ~50% success rate
}

// Unbiased uniform integer generation using ESP32 hardware RNG
// Returns number in [minInclusive, maxInclusive]
static inline int uniformIntInclusive(int minInclusive, int maxInclusive) {
  if (maxInclusive <= minInclusive) return minInclusive;
  uint32_t span = (uint32_t)(maxInclusive - minInclusive + 1);
  // Rejection sampling to avoid modulo bias
  uint32_t limit = UINT32_MAX - (UINT32_MAX % span);
  uint32_t r;
  do {
    r = esp_random();
  } while (r >= limit);
  return (int)(minInclusive + (r % span));
}

static inline int rollUnbiasedDie(int sides) {
  if (sides <= 1) return 1;
  return uniformIntInclusive(1, sides);
}

// Roll one die using the BG3 karmic algorithm (requires DC to compute p).
// If useKarmicDice is false, falls back to pure unbiased RNG.
int rollDie(int sides) {
  if (sides <= 1) return 1;

  int roll;

  if (useKarmicDice) {
    int   dc = effectiveDC(sides);
    float p  = (float)(sides - dc + 1) / (float)sides;  // prob of success

    if (karmicDebt > 0.0f) {
      // Hidden roll: does this succeed automatically?
      float autoProb  = p / (1.0f - karmicDebt);
      if (autoProb > 1.0f) autoProb = 1.0f;
      float hiddenRoll = (float)(esp_random() & 0xFFFF) / 65535.0f;

      if (hiddenRoll < autoProb) {
        // Auto success: pick random value from [dc, sides]
        roll = uniformIntInclusive(dc, sides);
        karmicDebt -= 2.0f * (1.0f - p);
        if (DEBUG_MODE)
          Serial.printf("[KARMA] AUTO SUCCESS roll=%d  dc=%d  p=%.3f  debt=%.4f\n",
                        roll, dc, p, karmicDebt);
        // Plotter
        if (DEBUG_MODE) Serial.printf("Roll:%d\tDebt:%.3f\n", roll, karmicDebt);
        return roll;
      }
    }

    // Normal roll
    roll = rollUnbiasedDie(sides);
    if (roll < dc) {
      // Failed: accumulate debt
      karmicDebt += p;
      if (DEBUG_MODE)
        Serial.printf("[KARMA] FAIL roll=%d  dc=%d  p=%.3f  debt+=%.3f -> %.4f\n",
                      roll, dc, p, p, karmicDebt);
    } else {
      if (DEBUG_MODE)
        Serial.printf("[KARMA] SUCCESS roll=%d  dc=%d  p=%.3f  debt=%.4f\n",
                      roll, dc, p, karmicDebt);
    }
  } else {
    roll = rollUnbiasedDie(sides);
  }

  // Serial Plotter
  if (DEBUG_MODE) {
    if (useKarmicDice)
      Serial.printf("Roll:%d\tDebt:%.3f\n", roll, karmicDebt);
    else
      Serial.printf("Roll:%d\n", roll);
  }

  return roll;
}

// Helper to draw perfectly centered button labels (vertical + horizontal)
void drawCenteredLabel(int x, int y, int w, int h, const char* label, uint8_t textSize, uint16_t color, uint16_t bg) {
  tft.setTextSize(textSize);
  tft.setTextColor(color, bg);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(label, x + w / 2, y + h / 2);
  tft.setTextDatum(TL_DATUM);
}

// Fixed 3D Animation System with correct polyhedra coordinates

// D4 - Regular tetrahedron vertices (centered and symmetric)
// Using coordinates of a regular tetra at ±1, which keeps shape rigid
Point3D tetraVertices[4] = {
  { 1.0f,  1.0f,  1.0f},
  {-1.0f, -1.0f,  1.0f},
  {-1.0f,  1.0f, -1.0f},
  { 1.0f, -1.0f, -1.0f}
};

// Tetrahedron edges (6 edges) - FIXED
int tetraEdges[6][2] = {
  {0,1}, {0,2}, {0,3},  // From top to other vertices
  {1,2}, {1,3}, {2,3}   // Connect all base vertices
};

// Tetrahedron triangular faces (for solid rendering)
// Winding chosen consistently so normals point outward
int tetraFaces[4][3] = {
  {0,1,2},
  {0,3,1},
  {0,2,3},
  {1,3,2}
};

// D6 - Cube vertices (correctly centered)
Point3D cubeVertices[8] = {
  {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f}, 
  { 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f},  // back face
  {-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f}, 
  { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f}   // front face
};

// Cube edges (12 edges)
int cubeEdges[12][2] = {
  {0,1}, {1,2}, {2,3}, {3,0},  // back face
  {4,5}, {5,6}, {6,7}, {7,4},  // front face
  {0,4}, {1,5}, {2,6}, {3,7}   // connecting edges
};

// Cube faces split into triangles (12 triangles total), outward winding
int cubeTriFaces[12][3] = {
  // Front z=+0.5
  {4,5,6}, {4,6,7},
  // Back z=-0.5 (reverse to point -Z)
  {0,2,1}, {0,3,2},
  // Right x=+0.5
  {1,2,6}, {1,6,5},
  // Left x=-0.5 (reverse)
  {0,7,3}, {0,4,7},
  // Top y=+0.5
  {3,6,2}, {3,7,6},
  // Bottom y=-0.5
  {0,1,5}, {0,5,4}
};

// D8 - Octahedron vertices (FIXED - proper regular octahedron)
Point3D octaVertices[6] = {
  { 0.0f,  0.0f,  0.707f},  // Top
  { 0.0f,  0.0f, -0.707f},  // Bottom
  { 0.707f,  0.0f,  0.0f},  // Right
  {-0.707f,  0.0f,  0.0f},  // Left
  { 0.0f,  0.707f,  0.0f},  // Front
  { 0.0f, -0.707f,  0.0f}   // Back
};

// Octahedron edges (12 edges) - FIXED
int octaEdges[12][2] = {
  {0,2}, {0,3}, {0,4}, {0,5},  // Top to sides
  {1,2}, {1,3}, {1,4}, {1,5},  // Bottom to sides  
  {2,4}, {4,3}, {3,5}, {5,2}   // Side square connections
};

// Octahedron faces (8 triangles), outward winding
int octaTriFaces[8][3] = {
  {0,2,4}, {0,4,3}, {0,3,5}, {0,5,2},
  {1,4,2}, {1,3,4}, {1,5,3}, {1,2,5}
};

// D10 - Pentagonal trapezohedron geometry (12 vertices)
Point3D d10Vertices[12]; // filled in initD10Geometry()

// D10 edges (30 edges)
int d10Edges[30][2];     // filled in initD10Geometry()

void initD10Geometry() {
  // Parameters (tweak for look)
  const float poleZ = 0.85f;
  const float ringZ = 0.22f;
  const float ringR = 0.62f;
  const float deg2rad = 0.01745329252f;

  // Poles
  d10Vertices[0] = {0.0f, 0.0f,  poleZ};
  d10Vertices[1] = {0.0f, 0.0f, -poleZ};

  // Upper ring (5 vertices)
  for (int i = 0; i < 5; i++) {
    float a = (72.0f * i) * deg2rad;
    d10Vertices[2 + i] = { ringR * cos(a), ringR * sin(a), ringZ };
  }

  // Lower ring (5 vertices), rotated by 36°
  for (int i = 0; i < 5; i++) {
    float a = (72.0f * i + 36.0f) * deg2rad;
    d10Vertices[7 + i] = { ringR * cos(a), ringR * sin(a), -ringZ };
  }

  // Build edges
  int k = 0;
  for (int i = 0; i < 5; i++) {
    int ui = 2 + i;
    int ui1 = 2 + ((i + 1) % 5);
    int li = 7 + i;
    int li1 = 7 + ((i + 1) % 5);
    // Pole connections
    d10Edges[k][0] = 0; d10Edges[k++][1] = ui;
    d10Edges[k][0] = 1; d10Edges[k++][1] = li;
    // Ring edges
    d10Edges[k][0] = ui;  d10Edges[k++][1] = ui1;
    d10Edges[k][0] = li;  d10Edges[k++][1] = li1;
    // Cross edges
    d10Edges[k][0] = ui;  d10Edges[k++][1] = li;
    d10Edges[k][0] = ui1; d10Edges[k++][1] = li;
  }
}

// D10 triangular faces (built in setup)
int d10TriFaces[20][3];
int d10TriCount = 0;

// D12 - Dodecahedron vertices (PROPER coordinates with normalization)
const float phi = 1.618034f;  // Golden ratio
const float norm = 0.525731f;  // Normalization factor

Point3D dodecaVertices[20] = {
  // 8 cube vertices (±1, ±1, ±1) normalized
  { 0.577f,  0.577f,  0.577f}, {-0.577f,  0.577f,  0.577f}, 
  { 0.577f, -0.577f,  0.577f}, {-0.577f, -0.577f,  0.577f},
  { 0.577f,  0.577f, -0.577f}, {-0.577f,  0.577f, -0.577f}, 
  { 0.577f, -0.577f, -0.577f}, {-0.577f, -0.577f, -0.577f},
  // 12 vertices on coordinate planes
  { 0.000f,  0.935f,  0.357f}, { 0.000f, -0.935f,  0.357f},  // XY plane
  { 0.000f,  0.935f, -0.357f}, { 0.000f, -0.935f, -0.357f},
  { 0.357f,  0.000f,  0.935f}, {-0.357f,  0.000f,  0.935f},  // XZ plane
  { 0.357f,  0.000f, -0.935f}, {-0.357f,  0.000f, -0.935f},
  { 0.935f,  0.357f,  0.000f}, {-0.935f,  0.357f,  0.000f},  // YZ plane
  { 0.935f, -0.357f,  0.000f}, {-0.935f, -0.357f,  0.000f}
};

// Dodecahedron edges (30 edges) - Proper connectivity
int dodecaEdges[30][2] = {
  // Each vertex connects to exactly 3 others
  {0,8}, {0,12}, {0,16}, {1,8}, {1,13}, {1,17}, {2,9}, {2,12}, 
  {2,18}, {3,9}, {3,13}, {3,19}, {4,10}, {4,14}, {4,16}, {5,10}, 
  {5,15}, {5,17}, {6,11}, {6,14}, {6,18}, {7,11}, {7,15}, {7,19},
  {8,10}, {9,11}, {12,13}, {14,15}, {16,18}, {17,19}
};

// Dodecahedron faces triangulated (built in setup)
int dodecaTriFaces[108][3]; // 36 faces but we may add reinforcement tris
int dodecaTriCount = 0;

// D20 - Icosahedron vertices (FIXED - proper icosahedron)
Point3D icosaVertices[12] = {
  // Standard icosahedron coordinates
  {-0.525f,  0.0f,    0.850f}, { 0.525f,  0.0f,    0.850f}, 
  {-0.525f,  0.0f,   -0.850f}, { 0.525f,  0.0f,   -0.850f},
  { 0.0f,    0.850f,  0.525f}, { 0.0f,    0.850f, -0.525f}, 
  { 0.0f,   -0.850f,  0.525f}, { 0.0f,   -0.850f, -0.525f},
  { 0.850f,  0.525f,  0.0f},   {-0.850f,  0.525f,  0.0f}, 
  { 0.850f, -0.525f,  0.0f},   {-0.850f, -0.525f,  0.0f}
};

// Icosahedron edges (30 edges) - FIXED
int icosaEdges[30][2] = {
  {0,1}, {0,4}, {0,6}, {0,9}, {0,11}, 
  {1,4}, {1,6}, {1,8}, {1,10},
  {2,3}, {2,5}, {2,7}, {2,9}, {2,11},
  {3,5}, {3,7}, {3,8}, {3,10},
  {4,5}, {4,8}, {4,9},
  {5,8}, {5,9},
  {6,7}, {6,10}, {6,11},
  {7,10}, {7,11},
  {8,10}, {9,11}
};

// Icosahedron faces (built by enumerating triangles from edges)
int icosaTriFaces[20][3];
int icosaTriCount = 0;

// Build icosahedron triangular faces from vertices and edge list
void buildIcosaFaces() {
  icosaTriCount = 0;
  const float PLANE_EPSILON = 1e-4f;
  for (int i = 0; i < 12; i++) {
    for (int j = i + 1; j < 12; j++) {
      if (!hasEdge(i, j, icosaEdges, 30)) continue;
      for (int k = j + 1; k < 12; k++) {
        if (!hasEdge(j, k, icosaEdges, 30)) continue;
        if (!hasEdge(k, i, icosaEdges, 30)) continue;

        Point3D vi = icosaVertices[i];
        Point3D vj = icosaVertices[j];
        Point3D vk = icosaVertices[k];
        float ux = vj.x - vi.x, uy = vj.y - vi.y, uz = vj.z - vi.z;
        float vx = vk.x - vi.x, vy = vk.y - vi.y, vz = vk.z - vi.z;
        float nx = uy * vz - uz * vy;
        float ny = uz * vx - ux * vz;
        float nz = ux * vy - uy * vx;

        // Skip nearly-degenerate triangles
        float nlen2 = nx*nx + ny*ny + nz*nz;
        if (nlen2 < 1e-6f) continue;

        bool allBack = true;
        bool allFront = true;
        for (int m = 0; m < 12; m++) {
          if (m == i || m == j || m == k) continue;
          Point3D vm = icosaVertices[m];
          float dx = vm.x - vi.x;
          float dy = vm.y - vi.y;
          float dz = vm.z - vi.z;
          float d = nx * dx + ny * dy + nz * dz;
          if (d > PLANE_EPSILON) allBack = false;   // point in front of plane
          if (d < -PLANE_EPSILON) allFront = false; // point behind plane
          if (!allBack && !allFront) break; // points on both sides, not a face
        }

        if (allBack || allFront) {
          // Ensure outward orientation relative to origin
          Point3D c = { (vi.x + vj.x + vk.x) / 3.0f,
                        (vi.y + vj.y + vk.y) / 3.0f,
                        (vi.z + vj.z + vk.z) / 3.0f };
          float outDot = nx * c.x + ny * c.y + nz * c.z;
          int a = i, b = j, cidx = k;
          if (outDot < 0.0f) { int tmp = b; b = cidx; cidx = tmp; }

          if (icosaTriCount < 20) {
            icosaTriFaces[icosaTriCount][0] = a;
            icosaTriFaces[icosaTriCount][1] = b;
            icosaTriFaces[icosaTriCount][2] = cidx;
            icosaTriCount++;
          }
        }
      }
    }
  }
}

// Animation state
float angleX = 0;
float angleY = 0;
float angleZ = 0;
bool animationActive = false;
bool isRolling = false;  // Track if we're in rolling animation
unsigned long lastAnimationTime = 0;
unsigned long animationStartTime  = 0;
unsigned long lastActivityTime    = 0;
const unsigned long SLEEP_TIMEOUT = 3UL * 60UL * 1000UL;

// Global orthographic scale so all dice share the same on-screen size
const float ORTHO_SCALE = 40.0f;  // tuned to roughly match D20 apparent size
// Per-die scale adjustment to visually match D20 footprint
const float D4_SCALE_FACTOR = 0.60f;  // shrink D4 further

void displayResults() {
  // Clear results area (top right, where ROLL button used to be)
  tft.fillRect(125, 5, 190, 35, TFT_BLACK);
  Serial.printf("Dice rolled since start: %d\n", diceRolledSinceStart);
  
  if (selectedDiceIndex == -1) return;
  
  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  
  // Display results at top right
  if (diceQuantity > 1) {
    // Show individual results and total in compact format
    tft.setCursor(130, 8);
    tft.print("Rolls: ");
    for (int i = 0; i < diceQuantity && i < 6; i++) { // Fewer to fit with bigger text
      tft.printf("%d ", rollResults[i]);
    }
    if (diceQuantity > 4 && diceQuantity < 7) tft.setTextSize(1);
    if (diceQuantity > 6) {
      tft.print("...");
    }
    // Show total on second line
    tft.setCursor(130, 24);
    tft.setTextSize(2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.printf("Total: %d", totalResult);
  } else {
    if (selectedDiceIndex == 5 && advState != ADV_NORMAL) {
      // D20 with advantage/disadvantage: show both dice, highlight the used one
      const char* advLabel = (advState == ADV_VANTAGGIO) ? "VANT." : "SVAN.";
      uint16_t c1 = (rollResults[0] == advRoll1) ? TFT_GREEN  : TFT_DARKGREY;
      uint16_t c2 = (rollResults[0] == advRoll2) ? TFT_GREEN  : TFT_DARKGREY;
      // Line 1: label + both values
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
      // Line 2: final result large
      tft.setCursor(130, 17);
      tft.setTextSize(2);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.printf("-> %d", rollResults[0]);
    } else {
      // Normal single die result
      tft.setCursor(130, 8);
      tft.setTextSize(2);
      tft.print("Result: ");
      tft.setTextSize(3);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.printf("%d", rollResults[0]);
    }
  }
}

// Fixed rotation with proper matrix multiplication
Point3D rotatePoint(Point3D point, float rx, float ry, float rz) {
  // Pre-calculate trigonometric values
  float cosX = cos(rx), sinX = sin(rx);
  float cosY = cos(ry), sinY = sin(ry);
  float cosZ = cos(rz), sinZ = sin(rz);
  
  Point3D result;
  
  // Apply rotations in order: X, then Y, then Z for stability
  // Rotate around X axis
  float y1 = point.y * cosX - point.z * sinX;
  float z1 = point.y * sinX + point.z * cosX;
  
  // Rotate around Y axis
  float x2 = point.x * cosY + z1 * sinY;
  float z2 = -point.x * sinY + z1 * cosY;
  
  // Rotate around Z axis
  result.x = x2 * cosZ - y1 * sinZ;
  result.y = x2 * sinZ + y1 * cosZ;
  result.z = z2;
  
  return result;
}

// Improved 3D to 2D projection
Point2D project3D(Point3D point) {
  int centerX = 220;  
  int centerY = 110;
  float scale = 140.0;  // DOUBLED from 70 to 140 for 2x size
  float distance = 3.0;  // Camera distance
  
  // Perspective projection
  float z = point.z + distance;
  if (z < 0.1f) z = 0.1f;  // Prevent division by zero
  
  float projX = (point.x * scale) / z;
  float projY = (point.y * scale) / z;
  
  Point2D result;
  result.x = centerX + (int)projX;
  result.y = centerY - (int)projY;
  return result;
}

// Orthographic projection (no perspective) — preserves shape
Point2D projectOrtho(Point3D point, float scale) {
  int centerX = 220;
  int centerY = 110;
  Point2D result;
  result.x = centerX + (int)(point.x * scale);
  result.y = centerY - (int)(point.y * scale);
  return result;
}

// Simple 16-bit (RGB565) color shading helper (0..1 factor)
static inline uint16_t shadeColor(uint16_t color, float factor) {
  if (factor < 0.0f) factor = 0.0f;
  if (factor > 1.0f) factor = 1.0f;
  uint8_t r = (color >> 11) & 0x1F;
  uint8_t g = (color >> 5) & 0x3F;
  uint8_t b = color & 0x1F;
  r = (uint8_t)(r * factor);
  g = (uint8_t)(g * factor);
  b = (uint8_t)(b * factor);
  return (uint16_t)((r << 11) | (g << 5) | b);
}

// Draw a solid D4 (tetrahedron) with painter's algorithm (back-to-front)
void drawSolidD4(Point3D rotated[4], Point2D projected[4], uint16_t baseColor) {
  struct FaceInfo { int a, b, c; float avgZ; float shade; } faces[4];

  // Precompute a simple directional light
  const float lx = 0.3f, ly = 0.6f, lz = 1.0f;
  const float llen = sqrtf(lx*lx + ly*ly + lz*lz);
  const float nxL = lx / llen, nyL = ly / llen, nzL = lz / llen;

  for (int i = 0; i < 4; i++) {
    int a = tetraFaces[i][0];
    int b = tetraFaces[i][1];
    int c = tetraFaces[i][2];

    Point3D v0 = rotated[a];
    Point3D v1 = rotated[b];
    Point3D v2 = rotated[c];

    // Face normal (right-hand rule)
    float ux = v1.x - v0.x, uy = v1.y - v0.y, uz = v1.z - v0.z;
    float vx = v2.x - v0.x, vy = v2.y - v0.y, vz = v2.z - v0.z;
    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;
    float nlen = sqrtf(nx*nx + ny*ny + nz*nz);
    if (nlen < 1e-6f) nlen = 1.0f;

    // Simple Lambert shading; flip normal toward viewer if needed
    float ndotl = (nx/ nlen) * nxL + (ny/ nlen) * nyL + (nz/ nlen) * nzL;
    if (ndotl < 0.0f) ndotl = 0.0f;  // no negative light
    float shade = 0.35f + 0.65f * ndotl; // keep minimum brightness

    float avgZ = (v0.z + v1.z + v2.z) / 3.0f;
    faces[i] = {a, b, c, avgZ, shade};
  }

  // Sort faces back-to-front (largest z first)
  for (int i = 0; i < 3; i++) {
    for (int j = i + 1; j < 4; j++) {
      if (faces[j].avgZ > faces[i].avgZ) {
        FaceInfo tmp = faces[i];
        faces[i] = faces[j];
        faces[j] = tmp;
      }
    }
  }

  // Draw filled triangles
  for (int i = 0; i < 4; i++) {
    Point2D p0 = projected[faces[i].a];
    Point2D p1 = projected[faces[i].b];
    Point2D p2 = projected[faces[i].c];
    uint16_t color = shadeColor(baseColor, faces[i].shade);
    tft.fillTriangle(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, color);
    // Optional edge overlay for crispness
    tft.drawLine(p0.x, p0.y, p1.x, p1.y, TFT_BLACK);
    tft.drawLine(p1.x, p1.y, p2.x, p2.y, TFT_BLACK);
    tft.drawLine(p2.x, p2.y, p0.x, p0.y, TFT_BLACK);
  }
}

// Simple 2px-thick line using offsets
static inline void drawThickLine2(int x0, int y0, int x1, int y1, uint16_t color) {
  tft.drawLine(x0, y0, x1, y1, color);
  tft.drawLine(x0 + 1, y0, x1 + 1, y1, color);
  tft.drawLine(x0, y0 + 1, x1, y1 + 1, color);
}

// Utility: check whether an undirected edge exists in an edge list
static inline bool hasEdge(int a, int b, int (*edges)[2], int numEdges) {
  for (int i = 0; i < numEdges; i++) {
    int e0 = edges[i][0];
    int e1 = edges[i][1];
    if ((e0 == a && e1 == b) || (e0 == b && e1 == a)) return true;
  }
  return false;
}

// Generic front-face-only wireframe using triangle faces
void drawWireframeFrontFaces(
  Point3D rotated[], Point2D projected[],
  int (*edges)[2], int numEdges,
  int (*triFaces)[3], int numTriFaces,
  uint16_t color
) {
  bool edgeVisible[64];
  for (int i = 0; i < 64; i++) edgeVisible[i] = false;
  const float FACE_EPSILON = 0.02f;
  const float viewX = 0.0f, viewY = 0.0f, viewZ = -1.0f; // viewer looking along -Z

  for (int f = 0; f < numTriFaces; f++) {
    int a = triFaces[f][0];
    int b = triFaces[f][1];
    int c = triFaces[f][2];

    Point3D v0 = rotated[a];
    Point3D v1 = rotated[b];
    Point3D v2 = rotated[c];

    float ux = v1.x - v0.x, uy = v1.y - v0.y, uz = v1.z - v0.z;
    float vx = v2.x - v0.x, vy = v2.y - v0.y, vz = v2.z - v0.z;
    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;
    // Ensure outward normal regardless of triangle winding by checking center
    Point3D ctri = { (v0.x + v1.x + v2.x) / 3.0f,
                     (v0.y + v1.y + v2.y) / 3.0f,
                     (v0.z + v1.z + v2.z) / 3.0f };
    float outDot = nx * ctri.x + ny * ctri.y + nz * ctri.z;
    if (outDot < 0.0f) { nx = -nx; ny = -ny; nz = -nz; }
    float facing = nx * viewX + ny * viewY + nz * viewZ;
    if (facing > FACE_EPSILON) {
      int faceEdges[3][2] = { {a,b}, {b,c}, {c,a} };
      for (int e = 0; e < 3; e++) {
        int u = faceEdges[e][0];
        int v = faceEdges[e][1];
        // Mark if the physical edge exists; otherwise draw the triangle edge anyway
        for (int i = 0; i < numEdges; i++) {
          int e0 = edges[i][0];
          int e1 = edges[i][1];
          if ((e0 == u && e1 == v) || (e0 == v && e1 == u)) {
            edgeVisible[i] = true;
            break;
          }
        }
      }
    }
  }

  for (int i = 0; i < numEdges; i++) {
    if (!edgeVisible[i]) continue;
    int a = edges[i][0];
    int b = edges[i][1];
    drawThickLine2(projected[a].x, projected[a].y, projected[b].x, projected[b].y, color);
  }
}

// Wireframe D4 showing only front-facing edges (orthographic)
void drawWireframeD4Ortho(Point3D rotated[4], uint16_t color) {
  // Project using orthographic with per-die scale
  Point2D proj[4];
  float scale = ORTHO_SCALE * D4_SCALE_FACTOR;
  for (int i = 0; i < 4; i++) proj[i] = projectOrtho(rotated[i], scale);

  // Mark edges that belong to front-facing faces only
  bool edgeVisible[6] = {false,false,false,false,false,false};
  const float FACE_EPSILON = 0.02f; // threshold to reduce flicker near silhouette
  const float viewX = 0.0f, viewY = 0.0f, viewZ = 1.0f; // looking along +Z

  for (int f = 0; f < 4; f++) {
    int a = tetraFaces[f][0];
    int b = tetraFaces[f][1];
    int c = tetraFaces[f][2];

    Point3D v0 = rotated[a];
    Point3D v1 = rotated[b];
    Point3D v2 = rotated[c];

    // Face normal
    float ux = v1.x - v0.x, uy = v1.y - v0.y, uz = v1.z - v0.z;
    float vx = v2.x - v0.x, vy = v2.y - v0.y, vz = v2.z - v0.z;
    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;

    // Facing if normal has positive dot with view direction
    float facing = nx * viewX + ny * viewY + nz * viewZ;
    if (facing > FACE_EPSILON) {
      int faceEdges[3][2] = { {a,b}, {b,c}, {c,a} };
      for (int e = 0; e < 3; e++) {
        int u = faceEdges[e][0];
        int v = faceEdges[e][1];
        for (int i = 0; i < 6; i++) {
          int e0 = tetraEdges[i][0];
          int e1 = tetraEdges[i][1];
          if ((e0 == u && e1 == v) || (e0 == v && e1 == u)) {
            edgeVisible[i] = true;
            break;
          }
        }
      }
    }
  }

  // Draw only visible edges (front-facing)
  for (int i = 0; i < 6; i++) {
    if (!edgeVisible[i]) continue;
    int a = tetraEdges[i][0];
    int b = tetraEdges[i][1];
    drawThickLine2(proj[a].x, proj[a].y, proj[b].x, proj[b].y, color);
  }
}

void animateDice() {
  if (selectedDiceIndex == -1) return;
  
  // Animation timing
  if (millis() - lastAnimationTime < 50) return;  // 20 FPS
  lastAnimationTime = millis();
  
  // Clear animation area
  tft.fillRect(125, 45, 190, 120, TFT_BLACK);
  
  // Clip drawing to the dice area to prevent edge popping/flicker
  // Use absolute coordinates (vpDatum=false) so our projected points remain screen-based
  tft.setViewport(125, 45, 190, 120, false);
  
  // Variable rotation speed based on state
  if (isRolling) {
    // Fast spin when rolling
    angleX += 0.3;
    angleY += 0.25;
    angleZ += 0.2;
  } else if (animationActive) {
    // Slow down after roll
    float slowdownFactor = 1.0 - ((millis() - animationStartTime - 1000) / 500.0);
    if (slowdownFactor < 0.1) slowdownFactor = 0.1;
    angleX += 0.3 * slowdownFactor;
    angleY += 0.25 * slowdownFactor;
    angleZ += 0.2 * slowdownFactor;
  } else {
    // Slow idle spin
    angleX += 0.02;
    angleY += 0.015;
    angleZ += 0.01;
  }
  
  // Choose geometry based on dice type
  uint16_t color;
  Point3D* vertices;
  int numVertices;
  int (*edges)[2];
  int numEdges;
  
  switch(selectedDiceIndex) {
    case 0: // D4
      color = TFT_RED;
      vertices = tetraVertices;
      numVertices = 4;
      edges = tetraEdges;
      numEdges = 6;
      break;
    case 1: // D6
      color = TFT_GREEN;
      vertices = cubeVertices;
      numVertices = 8;
      edges = cubeEdges;
      numEdges = 12;
      break;
    case 2: // D8
      color = TFT_BLUE;
      vertices = octaVertices;
      numVertices = 6;
      edges = octaEdges;
      numEdges = 12;
      break;
    case 3: // D10
      color = TFT_CYAN;
      vertices = d10Vertices;
      numVertices = 12;
      edges = d10Edges;
      numEdges = 30;
      break;
    case 4: // D12
      color = TFT_MAGENTA;
      vertices = dodecaVertices;
      numVertices = 20;
      edges = dodecaEdges;
      numEdges = 30;
      break;
    case 5: // D20
      color = TFT_YELLOW;
      vertices = icosaVertices;
      numVertices = 12;
      edges = icosaEdges;
      numEdges = 30;
      break;
    default: // D100 (same as D20)
      color = TFT_WHITE;
      vertices = icosaVertices;
      numVertices = 12;
      edges = icosaEdges;
      numEdges = 30;
      break;
  }
  
  // Project all vertices (and keep rotated positions for face normals)
  Point3D rotatedVertices[20];
  Point2D projectedVertices[20];  // Max needed for dodecahedron
  for (int i = 0; i < numVertices; i++) {
    rotatedVertices[i] = rotatePoint(vertices[i], angleX, angleY, angleZ);
    // Use orthographic for consistent size
    projectedVertices[i] = projectOrtho(rotatedVertices[i], ORTHO_SCALE);
  }

  // D4: orthographic wireframe with hidden edges to preserve shape
  if (selectedDiceIndex == 0) {
    drawWireframeD4Ortho(rotatedVertices, color);
    // Restore full-screen drawing
    tft.resetViewport();
    return;
  }
  
  // Draw wireframe for other dice: front-facing only
  if (selectedDiceIndex == 1) { // D6
    drawWireframeFrontFaces(rotatedVertices, projectedVertices, cubeEdges, 12, cubeTriFaces, 12, color);
  } else if (selectedDiceIndex == 2) { // D8
    drawWireframeFrontFaces(rotatedVertices, projectedVertices, octaEdges, 12, octaTriFaces, 8, color);
  } else if (selectedDiceIndex == 3) { // D10
    drawWireframeFrontFaces(rotatedVertices, projectedVertices, d10Edges, 30, d10TriFaces, d10TriCount, color);
  } else if (selectedDiceIndex == 4) { // D12
    drawWireframeFrontFaces(rotatedVertices, projectedVertices, dodecaEdges, 30, dodecaTriFaces, dodecaTriCount, color);
  } else { // D20 and D100
    drawWireframeFrontFaces(rotatedVertices, projectedVertices, icosaEdges, 30, icosaTriFaces, icosaTriCount, color);
  }

  // Restore full-screen drawing
  tft.resetViewport();
}

bool resultsShown = false;

void rollDice() {
  if (selectedDiceIndex == -1) return;
  
  // Start 3D animation
  animationActive = true;
  isRolling = true;  // Start fast spin
  animationStartTime = millis();
  resultsShown = false;
  lastAnimationTime = millis();
  
  // Reset rotation angles for fresh animation
  angleX = 0;
  angleY = 0;
  angleZ = 0;

  // BG3 karma: debt persists across roll sessions (intentional — tracks long-term luck)
  
  totalResult = 0;
  int sides = diceSides[selectedDiceIndex];
  bool useAdv = (selectedDiceIndex == 5 && advState != ADV_NORMAL);

  for (int i = 0; i < diceQuantity; i++) {
    if (useAdv) {
      int r1 = rollDie(sides);
      int r2 = rollDie(sides);
      advRoll1 = r1; advRoll2 = r2;  // save for display
      rollResults[i] = (advState == ADV_VANTAGGIO) ? max(r1, r2) : min(r1, r2);
      // Always print both dice when advantage/disadvantage is active
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
  
  // Animation will play, then results will show
}

void updateQuantityDisplay() {
  // Gap between "-" (x=220,w=30) and "+" (x=270,w=30): x=250, w=20, h=35
  const int qx=250, qy=180, qw=20, qh=35;
  tft.fillRect(qx, qy, qw, qh, TFT_BLACK);
  uint8_t ts = (diceQuantity < 10) ? 2 : 1;  // shrink to size 1 for double digits
  tft.setTextSize(ts);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawNumber(diceQuantity, qx + qw / 2, qy + qh / 2);
  tft.setTextDatum(TL_DATUM);
}

void updateDiceSelection() {
  // Update all dice button colors to show selection
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

// ── LED state ─────────────────────────────────────────────────────────────────
// active-LOW: 0 = full ON, 255 = OFF
// Non-D20: cyan 20%.  D20: yellow/green/red full brightness per adv state.

void ledApplyState() {
  if (selectedDiceIndex != 5) {
    // Not D20 — fixed cyan at 20% brightness
    ledcWrite(LED_RED_PIN,   LED_DUTY_OFF);
    ledcWrite(LED_GREEN_PIN, LED_DUTY_ON);
    ledcWrite(LED_BLUE_PIN,  LED_DUTY_ON);
  } else {
    switch (advState) {
      case ADV_NORMAL:
        // Yellow = Red + Green full
        ledcWrite(LED_RED_PIN,   0);
        ledcWrite(LED_GREEN_PIN, 0);
        ledcWrite(LED_BLUE_PIN,  255);
        break;
      case ADV_VANTAGGIO:
        // Green full
        ledcWrite(LED_RED_PIN,   255);
        ledcWrite(LED_GREEN_PIN, 0);
        ledcWrite(LED_BLUE_PIN,  255);
        break;
      case ADV_SVANTAGGIO:
        // Red full
        ledcWrite(LED_RED_PIN,   0);
        ledcWrite(LED_GREEN_PIN, 255);
        ledcWrite(LED_BLUE_PIN,  255);
        break;
    }
  }
}

// Dice selection actions
void btn0_action() { selectedDiceIndex = 0; updateDiceSelection(); advState = ADV_NORMAL; updateAdvButton(); ledApplyState(); }
void btn1_action() { selectedDiceIndex = 1; updateDiceSelection(); advState = ADV_NORMAL; updateAdvButton(); ledApplyState(); }
void btn2_action() { selectedDiceIndex = 2; updateDiceSelection(); advState = ADV_NORMAL; updateAdvButton(); ledApplyState(); }
void btn3_action() { selectedDiceIndex = 3; updateDiceSelection(); advState = ADV_NORMAL; updateAdvButton(); ledApplyState(); }
void btn4_action() { selectedDiceIndex = 4; updateDiceSelection(); advState = ADV_NORMAL; updateAdvButton(); ledApplyState(); }
void btn5_action() { selectedDiceIndex = 5; updateDiceSelection(); advState = ADV_NORMAL; updateAdvButton(); ledApplyState(); }
void btn6_action() { selectedDiceIndex = 6; updateDiceSelection(); advState = ADV_NORMAL; updateAdvButton(); ledApplyState(); }

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

void (*btnActions[])() = {
  btn0_action, btn1_action, btn2_action, btn3_action,
  btn4_action, btn5_action, btn6_action
};

void updateAdvButton() {
  uint16_t fill; uint16_t tcol; const char* lbl;
  if (selectedDiceIndex != 5) {
    fill = 0x4208; tcol = 0x8410; lbl = "ADV  solo D20";  // dark grey, inactive
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
  if (selectedDiceIndex != 5) return;  // only active for D20
  advState = (AdvState)((advState + 1) % 3);
  updateAdvButton();
  ledApplyState();
  if (DEBUG_MODE)
    Serial.printf("[ADV] stato -> %s\n",
      advState==ADV_NORMAL?"NORMALE": advState==ADV_VANTAGGIO?"VANTAGGIO":"SVANTAGGIO");
}

void rngMode_action() {
  useKarmicDice = !useKarmicDice;
  // Reset debt when switching modes
  karmicDebt = 0.0f;
  // Redraw button
  uint16_t fill  = useKarmicDice ? TFT_GREEN  : (uint16_t)0x7BEF; // green or dark grey
  uint16_t tcol  = useKarmicDice ? TFT_BLACK  : TFT_WHITE;
  const char* lbl = useKarmicDice ? "KARMA ON" : "RNG PURO";
  rngModeBtn->initButtonUL(125, 218, 195, 20, TFT_BLACK, fill, tcol, "", 1);
  rngModeBtn->drawSmoothButton(false, 1, TFT_BLACK);
  drawCenteredLabel(125, 218, 195, 20, lbl, 1, tcol, fill);
}

void setupDiceButtons() {
  // Left column - Rectangular dice buttons
  int btnWidth = 110;
  int btnHeight = 30;
  int spacing = 3;
  int leftX = 5;
  int startY = 5;

  // Create dice buttons in left column
  for (int i = 0; i < buttonCount; i++) {
    int x = leftX;
    int y = startY + i * (btnHeight + spacing);

    // Store button position for later reference
    diceButtonPos[i] = {x, y, btnWidth, btnHeight};

    diceButtons[i] = new ButtonWidget(&tft);
    diceButtons[i]->initButtonUL(x, y, btnWidth, btnHeight, TFT_WHITE, TFT_BLUE, TFT_WHITE, "", 2);
    diceButtons[i]->setPressAction(btnActions[i]);
    diceButtons[i]->drawSmoothButton(false, 3, TFT_BLACK);
    drawCenteredLabel(x, y, btnWidth, btnHeight, diceLabels[i], 2, TFT_WHITE, TFT_BLUE);
  }
  
  // Right column - ROLL button at bottom
  int rollX = 125;
  int rollY = 180;
  rollBtn = new ButtonWidget(&tft);
  rollBtn->initButtonUL(rollX, rollY, 90, 35, TFT_WHITE, TFT_RED, TFT_WHITE, "", 3);
  rollBtn->setPressAction(roll_action);
  rollBtn->drawSmoothButton(false, 3, TFT_BLACK);
  drawCenteredLabel(rollX, rollY, 90, 35, "ROLL", 3, TFT_WHITE, TFT_RED);
  
  // Quantity controls next to ROLL button
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
  
  // Initial quantity display
  updateQuantityDisplay();

  // Advantage/Disadvantage toggle — thin strip above ROLL area
  advBtn = new ButtonWidget(&tft);
  advBtn->initButtonUL(125, 167, 195, 11, TFT_BLACK, (uint16_t)0x4208, (uint16_t)0x8410, "", 1);
  advBtn->setPressAction(advMode_action);
  advBtn->drawSmoothButton(false, 1, TFT_BLACK);
  drawCenteredLabel(125, 167, 195, 11, "ADV  solo D20", 1, (uint16_t)0x8410, (uint16_t)0x4208);

  // RNG mode toggle button — bottom strip of right column
  rngModeBtn = new ButtonWidget(&tft);
  rngModeBtn->initButtonUL(125, 218, 195, 20, TFT_BLACK, (uint16_t)0x7BEF, TFT_WHITE, "", 1);
  rngModeBtn->setPressAction(rngMode_action);
  rngModeBtn->drawSmoothButton(false, 1, TFT_BLACK);
  drawCenteredLabel(125, 218, 195, 20, "RNG PURO", 1, TFT_WHITE, (uint16_t)0x7BEF);
}


// ── OTA page (served at http://<ip>/) ────────────────────────────────────────
static const char OTA_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>DnD Roller OTA</title>
<style>
  body{font-family:sans-serif;background:#1a1a2e;color:#eee;text-align:center;padding:40px}
  h1{color:#e94560}
  .card{background:#16213e;border-radius:12px;padding:30px;max-width:480px;margin:0 auto}
  input[type=file]{margin:16px 0;color:#aaa}
  .btn{background:#e94560;color:#fff;border:none;padding:12px 32px;border-radius:6px;
       cursor:pointer;font-size:16px;margin-top:16px}
  .btn:hover{background:#c73652}
  progress{width:90%;height:22px;margin:16px auto;display:none;border-radius:4px}
  #status{margin-top:12px;font-size:14px;color:#aaa;min-height:20px}
</style></head><body>
<div class="card">
  <h1>&#127922; DnD Roller OTA</h1>
  <p>Seleziona il <b>.bin</b> esportato da Arduino IDE<br>
  <small>(Sketch → Esporta binario compilato)</small></p>
  <input type="file" id="file" accept=".bin">
  <br>
  <button class="btn" onclick="upload()">Flasha firmware</button>
  <br>
  <progress id="prog" max="100" value="0"></progress>
  <div id="status"></div>
</div>
<script>
function upload(){
  var f=document.getElementById('file').files[0];
  if(!f){alert('Seleziona un file .bin');return;}
  var fd=new FormData();fd.append('firmware',f);
  var xhr=new XMLHttpRequest();
  xhr.upload.onprogress=function(e){
    var p=Math.round(e.loaded/e.total*100);
    document.getElementById('prog').style.display='block';
    document.getElementById('prog').value=p;
    document.getElementById('status').textContent='Caricamento: '+p+'%';
  };
  xhr.onload=function(){
    if(xhr.status===200){
      document.getElementById('status').textContent='✓ Completato! Il dispositivo si riavvierà...';
      document.getElementById('prog').value=100;
    } else {
      document.getElementById('status').textContent='✗ Errore: '+xhr.responseText;
    }
  };
  xhr.onerror=function(){
    document.getElementById('status').textContent='✗ Connessione persa';
  };
  xhr.open('POST','/update');xhr.send(fd);
}
</script></body></html>
)rawhtml";

void otaHandleRoot() {
  otaServer.send_P(200, "text/html", OTA_HTML);
}

void otaHandleUpload() {
  HTTPUpload& upload = otaServer.upload();

  if (upload.status == UPLOAD_FILE_START) {
    otaInProgress = true;
    // Show OTA screen on TFT
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("OTA Update", 160, 90);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(upload.filename.c_str(), 160, 115);
    Serial.printf("[OTA] Avvio: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
    // Animated progress: scrolling bar + KB counter
    // (contentLength not available in ESP32 core 3.x HTTPUpload)
    static uint8_t barTick = 0;
    barTick = (barTick + 6) % 200;
    tft.fillRect(40, 130, 240, 16, tft.color565(30, 30, 60));
    tft.fillRect(40 + barTick, 130, 60, 16, TFT_CYAN);
    char kbuf[24];
    snprintf(kbuf, sizeof(kbuf), "%u KB", (unsigned)(upload.totalSize / 1024));
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.fillRect(120, 152, 80, 10, TFT_BLACK);
    tft.drawString(kbuf, 160, 155);

  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("[OTA] Completato: %u byte\n", upload.totalSize);
      tft.fillRect(40, 130, 240, 16, TFT_GREEN);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.setTextSize(2);
      tft.drawString("OK! Riavvio...", 160, 165);
    } else {
      Update.printError(Serial);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.setTextSize(1);
      tft.drawString("ERRORE OTA", 160, 155);
    }
  }
}

void otaHandleUpdateEnd() {
  otaServer.sendHeader("Connection", "close");
  if (Update.hasError()) {
    otaServer.send(500, "text/plain", String("ERRORE: ") + Update.errorString());
  } else {
    otaServer.send(200, "text/plain", "OK");
    delay(1500);
    ESP.restart();
  }
}

void setupOTA() {
  otaServer.on("/",       HTTP_GET,  otaHandleRoot);
  otaServer.on("/update", HTTP_POST, otaHandleUpdateEnd, otaHandleUpload);
  otaServer.begin();
  Serial.printf("[OTA] Server avviato su http://%s/\n", deviceIP.c_str());
}

// WiFi + OTA init — called from setup()
void setupWiFi() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Connessione WiFi...", 160, 110);
  tft.drawString(WIFI_SSID, 160, 125);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) {
    delay(300);
    tft.drawString(".", 160 + (int)((millis()-t0)/300) * 4, 140);
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    deviceIP = WiFi.localIP().toString();
    setupOTA();

    // Show IP briefly
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("WiFi OK!", 160, 90);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("OTA: http://" + deviceIP + "/", 160, 118);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Avvio tra 3s...", 160, 138);
    Serial.printf("[WiFi] Connesso! IP: %s\n", deviceIP.c_str());
    delay(3000);
    tft.fillScreen(TFT_BLACK);

  } else {
    // WiFi failed — continue without OTA
    Serial.println("[WiFi] Timeout connessione — OTA non disponibile");
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("WiFi non disponibile", 160, 110);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Avvio offline...", 160, 128);
    delay(2000);
    tft.fillScreen(TFT_BLACK);

  }
}

void touch_calibrate() {
  uint16_t calData[5];
  bool calDataOK = false;

  if (!LittleFS.begin()) {
    LittleFS.format();
    LittleFS.begin();
  }

  if (LittleFS.exists(CALIBRATION_FILE)) {
    if (!REPEAT_CAL) {
      File f = LittleFS.open(CALIBRATION_FILE, "r");
      if (f && f.readBytes((char *)calData, sizeof(calData)) == sizeof(calData))
        calDataOK = true;
      f.close();
    } else {
      LittleFS.remove(CALIBRATION_FILE);
    }
  }

  if (calDataOK) {
    tft.setTouch(calData);
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 0);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.println("Touch corners as indicated");
    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);
    File f = LittleFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char *)calData, sizeof(calData));
      f.close();
    }
    tft.setTouch(calData);
  }
}

void setup() {
  Serial.begin(115200);
  // esp_random() used for dice — randomSeed() not needed
  if (DEBUG_MODE) {
    Serial.println("=== DnD Roller DEBUG MODE ===");
    Serial.println("Serial Plotter: Roll / KarmaAvg");
  }
  tft.begin();
  tft.setRotation(1);  // Landscape 90° right
  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(1);

  // Initialize generated geometry/face tables
  initD10Geometry();
  // Build D10 faces (two triangles per kite face)
  d10TriCount = 0;
  for (int i = 0; i < 5; i++) {
    int ui  = 2 + i;
    int ui1 = 2 + ((i + 1) % 5);
    int li  = 7 + i;
    // upper kite split
    d10TriFaces[d10TriCount][0] = 0; d10TriFaces[d10TriCount][1] = ui;  d10TriFaces[d10TriCount++][2] = li;
    d10TriFaces[d10TriCount][0] = 0; d10TriFaces[d10TriCount][1] = li;  d10TriFaces[d10TriCount++][2] = ui1;
    // lower kite split (toward bottom pole)
    d10TriFaces[d10TriCount][0] = 1; d10TriFaces[d10TriCount][1] = li;  d10TriFaces[d10TriCount++][2] = ui;
    d10TriFaces[d10TriCount][0] = 1; d10TriFaces[d10TriCount][1] = ui;  d10TriFaces[d10TriCount++][2] = ((7 + ((i + 4) % 5))); // previous lower ring vertex
  }

  // Dodeca triangulation from known pentagon cycles
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
    // Add reinforcement triangles to improve culling stability near pentagon center
    int v1 = pentagons[p][1], v2 = pentagons[p][2], v3 = pentagons[p][3], v4 = pentagons[p][4];
    dodecaTriFaces[dodecaTriCount][0] = v1; dodecaTriFaces[dodecaTriCount][1] = v2; dodecaTriFaces[dodecaTriCount++][2] = v3;
    dodecaTriFaces[dodecaTriCount][0] = v1; dodecaTriFaces[dodecaTriCount][1] = v3; dodecaTriFaces[dodecaTriCount++][2] = v4;
  }

  // Build robust icosa triangular faces from edges (avoids indexing mismatch)
  buildIcosaFaces();

  setupWiFi();
  touch_calibrate();

  // LED: PWM cyan at 20% brightness
  ledcAttach(LED_RED_PIN,   5000, 8);
  ledcAttach(LED_GREEN_PIN, 5000, 8);
  ledcAttach(LED_BLUE_PIN,  5000, 8);
  ledApplyState();  // cyan at 20% (no D20 selected yet)

  lastActivityTime = millis();
  setupDiceButtons();
}


// ── Serial command handler ────────────────────────────────────────────────────
// Available commands (send via Serial Monitor, line ending: Newline):
//   d4 d6 d8 d10 d12 d20 d100  — select dice
//   roll                        — execute a roll
//   qty <1-10>                  — set dice quantity
//   adv / disadv / normal       — set advantage state (D20 only)
//   karma on / karma off        — toggle karmic system
//   karma reset                 — reset karma history
//   status                      — print current system state

void printStatus() {
  const char* diceNames[] = {"D4","D6","D8","D10","D12","D20","D100"};
  Serial.println("──────────── STATUS ────────────");
  Serial.printf("  Dado:      %s\n", selectedDiceIndex >= 0 ? diceNames[selectedDiceIndex] : "nessuno");
  Serial.printf("  Quantita:  %d\n", diceQuantity);
  Serial.printf("  Karma:     %s\n", useKarmicDice ? "ON" : "OFF");
  if (useKarmicDice) {
    int sides = (selectedDiceIndex >= 0) ? diceSides[selectedDiceIndex] : 20;
    int dc    = effectiveDC(sides);
    float p   = (float)(sides - dc + 1) / (float)sides;
    Serial.printf("  Debt:      %.4f\n", karmicDebt);
    Serial.printf("  DC:        %d %s  (p=%.1f%%)\n",
                  dc, targetDC < 0 ? "(auto)" : "(manuale)", p * 100.0f);
    Serial.println("  Nota: debt>0 aumenta chance successo automatico");
  }
  const char* advNames[] = {"NORMALE", "VANTAGGIO", "SVANTAGGIO"};
  Serial.printf("  Vantaggio: %s\n", advNames[advState]);
  Serial.printf("  DebugMode: %s\n", DEBUG_MODE ? "ON" : "OFF");
  if (wifiConnected)
    Serial.printf("  OTA:       http://%s/\n", deviceIP.c_str());
  else
    Serial.println("  OTA:       non disponibile");
  Serial.println("────────────────────────────────");
}

void handleSerialCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();

  // ── Dice selection ──
  if      (cmd == "d4")   { btn0_action(); Serial.println("[CMD] Selezionato D4"); }
  else if (cmd == "d6")   { btn1_action(); Serial.println("[CMD] Selezionato D6"); }
  else if (cmd == "d8")   { btn2_action(); Serial.println("[CMD] Selezionato D8"); }
  else if (cmd == "d10")  { btn3_action(); Serial.println("[CMD] Selezionato D10"); }
  else if (cmd == "d12")  { btn4_action(); Serial.println("[CMD] Selezionato D12"); }
  else if (cmd == "d20")  { btn5_action(); Serial.println("[CMD] Selezionato D20"); }
  else if (cmd == "d100") { btn6_action(); Serial.println("[CMD] Selezionato D100"); }

  // ── Roll ──
  else if (cmd == "roll") {
    if (selectedDiceIndex == -1) Serial.println("[CMD] Errore: nessun dado selezionato");
    else { roll_action(); Serial.println("[CMD] Lancio eseguito"); }
  }

  // ── Quantity: qty <n> ──
  else if (cmd.startsWith("qty ")) {
    int val = cmd.substring(4).toInt();
    if (val < 1 || val > 10) {
      Serial.printf("[CMD] Errore: quantita' deve essere 1-10 (ricevuto: %d)\n", val);
    } else {
      diceQuantity = val;
      updateQuantityDisplay();
      Serial.printf("[CMD] Quantita' impostata a %d\n", diceQuantity);
    }
  }

  // ── Advantage ──
  else if (cmd == "adv") {
    if (selectedDiceIndex != 5) Serial.println("[CMD] Vantaggio disponibile solo per D20");
    else { advState = ADV_VANTAGGIO; updateAdvButton(); ledApplyState(); Serial.println("[CMD] Vantaggio attivato"); }
  }
  else if (cmd == "disadv") {
    if (selectedDiceIndex != 5) Serial.println("[CMD] Svantaggio disponibile solo per D20");
    else { advState = ADV_SVANTAGGIO; updateAdvButton(); ledApplyState(); Serial.println("[CMD] Svantaggio attivato"); }
  }
  else if (cmd == "normal") {
    advState = ADV_NORMAL; updateAdvButton(); ledApplyState();
    Serial.println("[CMD] Tiro normale");
  }

  // ── Karma ──
  else if (cmd == "karma on")  {
    if (!useKarmicDice) rngMode_action();
    Serial.println("[CMD] Karma ON");
  }
  else if (cmd == "karma off") {
    if (useKarmicDice) rngMode_action();
    Serial.println("[CMD] Karma OFF");
  }
  else if (cmd == "karma reset") {
    karmicDebt = 0.0f;
    Serial.println("[CMD] Debt karmico azzerato");
  }
  // dc <n>: set difficulty class
  else if (cmd.startsWith("dc ")) {
    int val = cmd.substring(3).toInt();
    int sides = (selectedDiceIndex >= 0) ? diceSides[selectedDiceIndex] : 20;
    if (val < 1 || val > sides) {
      Serial.printf("[CMD] Errore: DC deve essere 1-%d per il dado selezionato\n", sides);
    } else {
      targetDC = val;
      float p = (float)(sides - targetDC + 1) / (float)sides;
      Serial.printf("[CMD] DC impostato a %d  (p=%.1f%%)\n", targetDC, p * 100.0f);
    }
  }
  else if (cmd == "dc auto") {
    targetDC = -1;
    Serial.println("[CMD] DC impostato su auto (mediana+1)");
  }

  // ── OTA / IP ──
  else if (cmd == "ip" || cmd == "ota") {
    if (wifiConnected) {
      Serial.printf("[OTA] http://%s/\n", deviceIP.c_str());
    } else {
      Serial.println("[OTA] WiFi non connesso");
    }
  }

  // ── Status ──
  else if (cmd == "status") { printStatus(); }

  // ── Help ──
  else if (cmd == "help" || cmd == "?") {
    Serial.println("Comandi disponibili:");
    Serial.println("  d4 d6 d8 d10 d12 d20 d100  — seleziona dado");
    Serial.println("  roll                        — esegui lancio");
    Serial.println("  qty <1-20>                  — imposta quantita'");
    Serial.println("  adv / disadv / normal       — vantaggio D20");
    Serial.println("  karma on/off/reset          — sistema karmico BG3");
    Serial.println("  dc <n> / dc auto            — imposta Difficulty Class");
    Serial.println("  status                      — stato sistema");
    Serial.println("  ip / ota                    — mostra indirizzo OTA");
  }

  else {
    Serial.printf("[CMD] Comando non riconosciuto: '%s' (digita 'help' per la lista)\n", cmd.c_str());
  }
}

void loop() {
  // ── OTA handler ──
  if (wifiConnected && !otaInProgress) otaServer.handleClient();

  // ── Serial command input ──
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    handleSerialCommand(cmd);
  }

  // Run dice animation continuously when a dice is selected
  if (selectedDiceIndex != -1 && !otaInProgress) {
    animateDice();
    
    // Check if rolling animation should transition to slowdown
    if (isRolling && (millis() - animationStartTime > 1000)) {
      isRolling = false;  // Stop fast spin after 2 seconds
    }
    
    // Check if animation should end and show results
    if (!resultsShown && animationActive && (millis() - animationStartTime > 1500)) {
      animationActive = false;
      resultsShown = true;
      displayResults();
    }
  }
  
  static uint32_t lastScan        = 0;
  static uint32_t lastPressTime   = 0;
  static bool     repeatTriggered = false;

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
            // First press: fire immediately
            allButtons[i]->press(true);
            allButtons[i]->pressAction();
            lastPressTime   = millis();
            repeatTriggered = false;
          } else {
            // Held: auto-repeat only for quantity buttons
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

    // Auto-sleep after 3 minutes of inactivity
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