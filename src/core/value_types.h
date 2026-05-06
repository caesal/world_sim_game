#ifndef WORLD_SIM_VALUE_TYPES_H
#define WORLD_SIM_VALUE_TYPES_H

typedef unsigned int Color32;

typedef struct {
    int x;
    int y;
} MapPoint;

#define COLOR32_RGB(r, g, b) ((Color32)(((r) & 0xff) | (((g) & 0xff) << 8) | (((b) & 0xff) << 16)))

#endif
