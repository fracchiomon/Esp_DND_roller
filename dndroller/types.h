#ifndef TYPES_H
#define TYPES_H

// ── 3D / 2D Geometry Types ──────────────────────────────────────────
struct Point3D {
  float x, y, z;
};

struct Point2D {
  int x, y;
};

// ── Button Position Storage ─────────────────────────────────────────
struct ButtonPos {
  int x, y, w, h;
};

// ── Advantage / Disadvantage State ───────────────────────────��──────
enum AdvState {
  ADV_NORMAL,
  ADV_VANTAGGIO,
  ADV_SVANTAGGIO
};

// ── Face Info for Painter's Algorithm ───────────────────────────────
struct FaceInfo {
  int a, b, c;
  float avgZ;
  float shade;
};

#endif