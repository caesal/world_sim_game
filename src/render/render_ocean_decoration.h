#ifndef WORLD_SIM_RENDER_OCEAN_DECORATION_H
#define WORLD_SIM_RENDER_OCEAN_DECORATION_H

#include "core/render_snapshot.h"
#include "ui/ui_types.h"
#include <windows.h>

typedef struct {
    int exterior_items;
    int interior_items;
    int exterior_rebuilds;
    int interior_rebuilds;
    int item_rebuilds;
    int compass_items;
    int interior_water_only;
    int interior_deep_only;
    int interior_shallow_allowed_seen;
    int same_type_spacing_ok;
    int interior_min_clearance;
    int motif_overlap_count;
    int motif_spacing_violation_count;
    int exterior_spacing_ok;
    int exterior_texture_score;
    int interior_texture_score;
    int texture_asset_ready;
    int motif_asset_ready;
    int primitive_wave_stamps;
    unsigned int item_hash;
    unsigned int motif_mask;
} OceanDecorationProbeInfo;

void render_ocean_decoration_draw_background(HDC hdc, RECT client, MapLayout layout,
                                             const RenderSnapshot *snapshot);
void render_ocean_decoration_draw_overlay(HDC hdc, RECT client, MapLayout layout,
                                          const RenderSnapshot *snapshot);
void render_ocean_decoration_draw(HDC hdc, RECT client, MapLayout layout,
                                  const RenderSnapshot *snapshot);
void render_ocean_decoration_reset_debug(void);
OceanDecorationProbeInfo render_ocean_decoration_probe_info(void);

#endif
