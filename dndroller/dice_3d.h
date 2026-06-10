#ifndef DICE_3D_H
#define DICE_3D_H

#include "types.h"

// ── 3D Transformations ──────────────────────────────────────────────
Point3D rotatePoint(Point3D point, float rx, float ry, float rz);
Point2D project3D(Point3D point);
Point2D projectOrtho(Point3D point, float scale);

// ── Rendering Utilities ────────────────────────────────────────────
uint16_t shadeColor(uint16_t color, float factor);
void drawThickLine2(int x0, int y0, int x1, int y1, uint16_t color);
bool hasEdge(int a, int b, int (*edges)[2], int numEdges);

// ── Drawing Functions ──────────────────────────────────────────────
void drawSolidD4(Point3D rotated[4], Point2D projected[4], uint16_t baseColor);
void drawWireframeFrontFaces(
  Point3D rotated[], Point2D projected[],
  int (*edges)[2], int numEdges,
  int (*triFaces)[3], int numTriFaces,
  uint16_t color
);
void drawWireframeD4Ortho(Point3D rotated[4], uint16_t color);

// ── Main Animation ─────────────────────────────────────────────────
void animateDice();

// ── Animation State (extern for access from main) ─────────────────
extern float angleX, angleY, angleZ;
extern bool animationActive, isRolling;
extern unsigned long animationStartTime, lastAnimationTime;

#endif