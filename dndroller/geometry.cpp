#include "geometry.h"
#include <math.h>

// ────────────────────────────────────────────────────────────────────
// D4 - Regular tetrahedron vertices (centered and symmetric)
// ──────��─────────────────────────────────────────────────────────────
Point3D tetraVertices[4] = {
  { 1.0f,  1.0f,  1.0f},
  {-1.0f, -1.0f,  1.0f},
  {-1.0f,  1.0f, -1.0f},
  { 1.0f, -1.0f, -1.0f}
};

int tetraEdges[6][2] = {
  {0,1}, {0,2}, {0,3},
  {1,2}, {1,3}, {2,3}
};

int tetraFaces[4][3] = {
  {0,1,2},
  {0,3,1},
  {0,2,3},
  {1,3,2}
};

// ────────────────────────────────────────────────────────────────────
// D6 - Cube vertices (correctly centered)
// ────────────────────────────────────────────────────────────────────
Point3D cubeVertices[8] = {
  {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f}, 
  { 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f},
  {-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f}, 
  { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f}
};

int cubeEdges[12][2] = {
  {0,1}, {1,2}, {2,3}, {3,0},
  {4,5}, {5,6}, {6,7}, {7,4},
  {0,4}, {1,5}, {2,6}, {3,7}
};

int cubeTriFaces[12][3] = {
  {4,5,6}, {4,6,7},
  {0,2,1}, {0,3,2},
  {1,2,6}, {1,6,5},
  {0,7,3}, {0,4,7},
  {3,6,2}, {3,7,6},
  {0,1,5}, {0,5,4}
};

// ────────────────────────────────────────────────────────────────────
// D8 - Octahedron vertices (proper regular octahedron)
// ────────────────────────────────────────────────────────────────────
Point3D octaVertices[6] = {
  { 0.0f,  0.0f,  0.707f},
  { 0.0f,  0.0f, -0.707f},
  { 0.707f,  0.0f,  0.0f},
  {-0.707f,  0.0f,  0.0f},
  { 0.0f,  0.707f,  0.0f},
  { 0.0f, -0.707f,  0.0f}
};

int octaEdges[12][2] = {
  {0,2}, {0,3}, {0,4}, {0,5},
  {1,2}, {1,3}, {1,4}, {1,5},  
  {2,4}, {4,3}, {3,5}, {5,2}
};

int octaTriFaces[8][3] = {
  {0,2,4}, {0,4,3}, {0,3,5}, {0,5,2},
  {1,4,2}, {1,3,4}, {1,5,3}, {1,2,5}
};

// ────────────────────────────────────────────────────────────────────
// D10 - Pentagonal trapezohedron
// ────────────────────────────────────────────────────────────────────
Point3D d10Vertices[12];
int d10Edges[30][2];
int d10TriFaces[20][3];
int d10TriCount = 0;

void initD10Geometry() {
  const float poleZ = 0.85f;
  const float ringZ = 0.22f;
  const float ringR = 0.62f;
  const float deg2rad = 0.01745329252f;

  d10Vertices[0] = {0.0f, 0.0f,  poleZ};
  d10Vertices[1] = {0.0f, 0.0f, -poleZ};

  for (int i = 0; i < 5; i++) {
    float a = (72.0f * i) * deg2rad;
    d10Vertices[2 + i] = { ringR * cos(a), ringR * sin(a), ringZ };
  }

  for (int i = 0; i < 5; i++) {
    float a = (72.0f * i + 36.0f) * deg2rad;
    d10Vertices[7 + i] = { ringR * cos(a), ringR * sin(a), -ringZ };
  }

  int k = 0;
  for (int i = 0; i < 5; i++) {
    int ui = 2 + i;
    int ui1 = 2 + ((i + 1) % 5);
    int li = 7 + i;
    int li1 = 7 + ((i + 1) % 5);
    
    d10Edges[k][0] = 0; d10Edges[k++][1] = ui;
    d10Edges[k][0] = 1; d10Edges[k++][1] = li;
    d10Edges[k][0] = ui;  d10Edges[k++][1] = ui1;
    d10Edges[k][0] = li;  d10Edges[k++][1] = li1;
    d10Edges[k][0] = ui;  d10Edges[k++][1] = li;
    d10Edges[k][0] = ui1; d10Edges[k++][1] = li;
  }
}

// ────────────────────────────────────────────────────────────────────
// D12 - Dodecahedron
// ────────────────────────────────────────────────────────────────────
const float phi = 1.618034f;
const float norm = 0.525731f;

Point3D dodecaVertices[20] = {
  { 0.577f,  0.577f,  0.577f}, {-0.577f,  0.577f,  0.577f}, 
  { 0.577f, -0.577f,  0.577f}, {-0.577f, -0.577f,  0.577f},
  { 0.577f,  0.577f, -0.577f}, {-0.577f,  0.577f, -0.577f}, 
  { 0.577f, -0.577f, -0.577f}, {-0.577f, -0.577f, -0.577f},
  { 0.000f,  0.935f,  0.357f}, { 0.000f, -0.935f,  0.357f},
  { 0.000f,  0.935f, -0.357f}, { 0.000f, -0.935f, -0.357f},
  { 0.357f,  0.000f,  0.935f}, {-0.357f,  0.000f,  0.935f},
  { 0.357f,  0.000f, -0.935f}, {-0.357f,  0.000f, -0.935f},
  { 0.935f,  0.357f,  0.000f}, {-0.935f,  0.357f,  0.000f},
  { 0.935f, -0.357f,  0.000f}, {-0.935f, -0.357f,  0.000f}
};

int dodecaEdges[30][2] = {
  {0,8}, {0,12}, {0,16}, {1,8}, {1,13}, {1,17}, {2,9}, {2,12}, 
  {2,18}, {3,9}, {3,13}, {3,19}, {4,10}, {4,14}, {4,16}, {5,10}, 
  {5,15}, {5,17}, {6,11}, {6,14}, {6,18}, {7,11}, {7,15}, {7,19},
  {8,10}, {9,11}, {12,13}, {14,15}, {16,18}, {17,19}
};

int dodecaTriFaces[108][3];
int dodecaTriCount = 0;

// ────────────────────────────────────────────────────────────────────
// D20 - Icosahedron
// ────────────────────────────────────────────────────────────────────
Point3D icosaVertices[12] = {
  {-0.525f,  0.0f,    0.850f}, { 0.525f,  0.0f,    0.850f}, 
  {-0.525f,  0.0f,   -0.850f}, { 0.525f,  0.0f,   -0.850f},
  { 0.0f,    0.850f,  0.525f}, { 0.0f,    0.850f, -0.525f}, 
  { 0.0f,   -0.850f,  0.525f}, { 0.0f,   -0.850f, -0.525f},
  { 0.850f,  0.525f,  0.0f},   {-0.850f,  0.525f,  0.0f}, 
  { 0.850f, -0.525f,  0.0f},   {-0.850f, -0.525f,  0.0f}
};

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

int icosaTriFaces[20][3];
int icosaTriCount = 0;

static inline bool hasEdge(int a, int b, int (*edges)[2], int numEdges) {
  for (int i = 0; i < numEdges; i++) {
    int e0 = edges[i][0];
    int e1 = edges[i][1];
    if ((e0 == a && e1 == b) || (e0 == b && e1 == a)) return true;
  }
  return false;
}

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
          if (d > PLANE_EPSILON) allBack = false;
          if (d < -PLANE_EPSILON) allFront = false;
          if (!allBack && !allFront) break;
        }

        if (allBack || allFront) {
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