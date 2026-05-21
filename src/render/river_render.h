#ifndef WORLD_SIM_RIVER_RENDER_H
#define WORLD_SIM_RIVER_RENDER_H

#include "platform/platform_types.h"

#include "render/river_geometry.h"
#include "ui/ui_layout.h"

void river_render_draw_layer(HDC hdc, RECT client, MapLayout layout);
const HydrologyRenderStats *river_render_stats(void);
void river_render_set_lod_tile_size(int tile_size);
int river_render_lod_bucket_for_tile_size(int tile_size);

#endif
