#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "types.h"

// ── D4 (Tetrahedron) ────────────────────────────────────────────────
extern Point3D tetraVertices[4];
extern int tetraEdges[6][2];
extern int tetraFaces[4][3];

// ── D6 (Cube) ───────────────────────────────────────────────────────
extern Point3D cubeVertices[8];
extern int cubeEdges[12][2];
extern int cubeTriFaces[12][3];

// ── D8 (Octahedron) ─────────────────────────────────────────────────
extern Point3D octaVertices[6];
extern int octaEdges[12][2];
extern int octaTriFaces[8][3];

// ── D10 (Pentagonal Trapezohedron) ──────────────────────────────────
extern Point3D d10Vertices[12];
extern int d10Edges[30][2];
extern int d10TriFaces[20][3];
extern int d10TriCount;
void initD10Geometry();

// ── D12 (Dodecahedron) ──────────────────────────────────────────────
extern const float phi;
extern const float norm;
extern Point3D dodecaVertices[20];
extern int dodecaEdges[30][2];
extern int dodecaTriFaces[108][3];
extern int dodecaTriCount;

// ── D20 (Icosahedron) ───────────────────────────────────────────────
extern Point3D icosaVertices[12];
extern int icosaEdges[30][2];
extern int icosaTriFaces[20][3];
extern int icosaTriCount;
void buildIcosaFaces();

#endif