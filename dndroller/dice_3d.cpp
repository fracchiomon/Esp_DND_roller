#include "dice_3d.h"
#include "geometry.h"
#include "config.h"
#include "types.h"
#include <math.h>

// Animation state
float angleX = 0;
float angleY = 0;
float angleZ = 0;
bool animationActive = false;
bool isRolling = false;
unsigned long lastAnimationTime = 0;
unsigned long animationStartTime = 0;
unsigned long lastAnimationTime_local = 0;

// Forward declaration needed for extern
extern TFT_eSPI tft;
extern int selectedDiceIndex;
extern int diceSides[7];
extern bool useKarmicDice;

Point3D rotatePoint(Point3D point, float rx, float ry, float rz) {
  float cosX = cos(rx), sinX = sin(rx);
  float cosY = cos(ry), sinY = sin(ry);
  float cosZ = cos(rz), sinZ = sin(rz);
  
  Point3D result;
  
  float y1 = point.y * cosX - point.z * sinX;
  float z1 = point.y * sinX + point.z * cosX;
  
  float x2 = point.x * cosY + z1 * sinY;
  float z2 = -point.x * sinY + z1 * cosY;
  
  result.x = x2 * cosZ - y1 * sinZ;
  result.y = x2 * sinZ + y1 * cosZ;
  result.z = z2;
  
  return result;
}

Point2D project3D(Point3D point) {
  int centerX = 220;  
  int centerY = 110;
  float scale = 140.0;
  float distance = 3.0;
  
  float z = point.z + distance;
  if (z < 0.1f) z = 0.1f;
  
  float projX = (point.x * scale) / z;
  float projY = (point.y * scale) / z;
  
  Point2D result;
  result.x = centerX + (int)projX;
  result.y = centerY - (int)projY;
  return result;
}

Point2D projectOrtho(Point3D point, float scale) {
  int centerX = 220;
  int centerY = 110;
  Point2D result;
  result.x = centerX + (int)(point.x * scale);
  result.y = centerY - (int)(point.y * scale);
  return result;
}

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

void drawSolidD4(Point3D rotated[4], Point2D projected[4], uint16_t baseColor) {
  FaceInfo faces[4];
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

    float ux = v1.x - v0.x, uy = v1.y - v0.y, uz = v1.z - v0.z;
    float vx = v2.x - v0.x, vy = v2.y - v0.y, vz = v2.z - v0.z;
    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;
    float nlen = sqrtf(nx*nx + ny*ny + nz*nz);
    if (nlen < 1e-6f) nlen = 1.0f;

    float ndotl = (nx/ nlen) * nxL + (ny/ nlen) * nyL + (nz/ nlen) * nzL;
    if (ndotl < 0.0f) ndotl = 0.0f;
    float shade = 0.35f + 0.65f * ndotl;

    float avgZ = (v0.z + v1.z + v2.z) / 3.0f;
    faces[i] = {a, b, c, avgZ, shade};
  }

  for (int i = 0; i < 3; i++) {
    for (int j = i + 1; j < 4; j++) {
      if (faces[j].avgZ > faces[i].avgZ) {
        FaceInfo tmp = faces[i];
        faces[i] = faces[j];
        faces[j] = tmp;
      }
    }
  }

  for (int i = 0; i < 4; i++) {
    Point2D p0 = projected[faces[i].a];
    Point2D p1 = projected[faces[i].b];
    Point2D p2 = projected[faces[i].c];
    uint16_t color = shadeColor(baseColor, faces[i].shade);
    tft.fillTriangle(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, color);
    tft.drawLine(p0.x, p0.y, p1.x, p1.y, TFT_BLACK);
    tft.drawLine(p1.x, p1.y, p2.x, p2.y, TFT_BLACK);
    tft.drawLine(p2.x, p2.y, p0.x, p0.y, TFT_BLACK);
  }
}

static inline void drawThickLine2(int x0, int y0, int x1, int y1, uint16_t color) {
  tft.drawLine(x0, y0, x1, y1, color);
  tft.drawLine(x0 + 1, y0, x1 + 1, y1, color);
  tft.drawLine(x0, y0 + 1, x1, y1 + 1, color);
}

bool hasEdge(int a, int b, int (*edges)[2], int numEdges) {
  for (int i = 0; i < numEdges; i++) {
    int e0 = edges[i][0];
    int e1 = edges[i][1];
    if ((e0 == a && e1 == b) || (e0 == b && e1 == a)) return true;
  }
  return false;
}

void drawWireframeFrontFaces(
  Point3D rotated[], Point2D projected[],
  int (*edges)[2], int numEdges,
  int (*triFaces)[3], int numTriFaces,
  uint16_t color
) {
  bool edgeVisible[64];
  for (int i = 0; i < 64; i++) edgeVisible[i] = false;
  const float FACE_EPSILON = 0.02f;
  const float viewX = 0.0f, viewY = 0.0f, viewZ = -1.0f;

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

void drawWireframeD4Ortho(Point3D rotated[4], uint16_t color) {
  Point2D proj[4];
  float scale = ORTHO_SCALE * D4_SCALE_FACTOR;
  for (int i = 0; i < 4; i++) proj[i] = projectOrtho(rotated[i], scale);

  bool edgeVisible[6] = {false,false,false,false,false,false};
  const float FACE_EPSILON = 0.02f;
  const float viewX = 0.0f, viewY = 0.0f, viewZ = 1.0f;

  for (int f = 0; f < 4; f++) {
    int a = tetraFaces[f][0];
    int b = tetraFaces[f][1];
    int c = tetraFaces[f][2];

    Point3D v0 = rotated[a];
    Point3D v1 = rotated[b];
    Point3D v2 = rotated[c];

    float ux = v1.x - v0.x, uy = v1.y - v0.y, uz = v1.z - v0.z;
    float vx = v2.x - v0.x, vy = v2.y - v0.y, vz = v2.z - v0.z;
    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;

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

  for (int i = 0; i < 6; i++) {
    if (!edgeVisible[i]) continue;
    int a = tetraEdges[i][0];
    int b = tetraEdges[i][1];
    drawThickLine2(proj[a].x, proj[a].y, proj[b].x, proj[b].y, color);
  }
}

void animateDice() {
  if (selectedDiceIndex == -1) return;
  
  if (millis() - lastAnimationTime < 50) return;
  lastAnimationTime = millis();
  
  tft.fillRect(125, 45, 190, 120, TFT_BLACK);
  tft.setViewport(125, 45, 190, 120, false);
  
  if (isRolling) {
    angleX += 0.3;
    angleY += 0.25;
    angleZ += 0.2;
  } else if (animationActive) {
    float slowdownFactor = 1.0 - ((millis() - animationStartTime - 1000) / 500.0);
    if (slowdownFactor < 0.1) slowdownFactor = 0.1;
    angleX += 0.3 * slowdownFactor;
    angleY += 0.25 * slowdownFactor;
    angleZ += 0.2 * slowdownFactor;
  } else {
    angleX += 0.02;
    angleY += 0.015;
    angleZ += 0.01;
  }
  
  uint16_t color;
  Point3D* vertices;
  int numVertices;
  int (*edges)[2];
  int numEdges;
  
  switch(selectedDiceIndex) {
    case 0: color = TFT_RED; vertices = tetraVertices; numVertices = 4; edges = tetraEdges; numEdges = 6; break;
    case 1: color = TFT_GREEN; vertices = cubeVertices; numVertices = 8; edges = cubeEdges; numEdges = 12; break;
    case 2: color = TFT_BLUE; vertices = octaVertices; numVertices = 6; edges = octaEdges; numEdges = 12; break;
    case 3: color = TFT_CYAN; vertices = d10Vertices; numVertices = 12; edges = d10Edges; numEdges = 30; break;
    case 4: color = TFT_MAGENTA; vertices = dodecaVertices; numVertices = 20; edges = dodecaEdges; numEdges = 30; break;
    case 5: color = TFT_YELLOW; vertices = icosaVertices; numVertices = 12; edges = icosaEdges; numEdges = 30; break;
    default: color = TFT_WHITE; vertices = icosaVertices; numVertices = 12; edges = icosaEdges; numEdges = 30; break;
  }
  
  Point3D rotatedVertices[20];
  Point2D projectedVertices[20];
  for (int i = 0; i < numVertices; i++) {
    rotatedVertices[i] = rotatePoint(vertices[i], angleX, angleY, angleZ);
    projectedVertices[i] = projectOrtho(rotatedVertices[i], ORTHO_SCALE);
  }

  if (selectedDiceIndex == 0) {
    drawWireframeD4Ortho(rotatedVertices, color);
    tft.resetViewport();
    return;
  }
  
  if (selectedDiceIndex == 1) { 
    drawWireframeFrontFaces(rotatedVertices, projectedVertices, cubeEdges, 12, cubeTriFaces, 12, color);
  } else if (selectedDiceIndex == 2) {
    drawWireframeFrontFaces(rotatedVertices, projectedVertices, octaEdges, 12, octaTriFaces, 8, color);
  } else if (selectedDiceIndex == 3) {
    drawWireframeFrontFaces(rotatedVertices, projectedVertices, d10Edges, 30, d10TriFaces, d10TriCount, color);
  } else if (selectedDiceIndex == 4) {
    drawWireframeFrontFaces(rotatedVertices, projectedVertices, dodecaEdges, 30, dodecaTriFaces, dodecaTriCount, color);
  } else {
    drawWireframeFrontFaces(rotatedVertices, projectedVertices, icosaEdges, 30, icosaTriFaces, icosaTriCount, color);
  }

  tft.resetViewport();
}