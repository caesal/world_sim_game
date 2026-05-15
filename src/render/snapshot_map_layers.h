#ifndef WORLD_SIM_SNAPSHOT_MAP_LAYERS_H
#define WORLD_SIM_SNAPSHOT_MAP_LAYERS_H

#include "render/render_common.h"

void draw_snapshot_terrain_layer(HDC hdc, RECT client, MapLayout layout);
void draw_snapshot_coast_layer(HDC hdc, RECT client, MapLayout layout);
void draw_snapshot_political_layer(HDC hdc, RECT client, MapLayout layout);
void draw_snapshot_hydrology_layer(HDC hdc, RECT client, MapLayout layout);
void draw_snapshot_border_layer(HDC hdc, RECT client, MapLayout layout);

#endif
