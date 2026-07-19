#ifndef WORLD_SIM_RIVER_RENDER_H
#define WORLD_SIM_RIVER_RENDER_H

#include <windows.h>

#include "render/river_geometry.h"
#include "render/river_presentation_filter.h"
#include "ui/ui_layout.h"

void river_render_draw_layer(HDC hdc, RECT client, MapLayout layout,
                             const RenderSnapshot *snapshot);
int river_render_draw_layer_lod(HDC hdc, RECT client, MapLayout layout,
                                const RenderSnapshot *snapshot, int lod);
const HydrologyRenderStats *river_render_stats(void);
void river_render_set_lod_tile_size(int tile_size);
int river_render_lod_bucket_for_tile_size(int tile_size);
int river_render_lod_bucket_for_zoom(int zoom_percent, int tile_size);
int river_render_path_visible_at_lod(const RiverRenderPath *path, int lod);
RiverPresentationFilterMetrics river_render_close_filter_metrics(void);

#endif
