#ifndef WORLD_SIM_RENDER_OCEAN_DECORATION_H
#define WORLD_SIM_RENDER_OCEAN_DECORATION_H

#include "core/render_snapshot.h"
#include "render/render_layer_cache.h"
#include "ui/ui_types.h"
#include <stdint.h>
#include <windows.h>

typedef struct {
    uint64_t composite_rebuilds;
    uint64_t composite_presents;
    int interior_water_only;
    int interior_deep_only;
    int interior_shallow_allowed_seen;
    int interior_min_clearance;
    uint64_t exterior_layer_rebuilds;
    uint64_t interior_layer_rebuilds;
    uint64_t exterior_layer_presents;
    uint64_t interior_layer_presents;
    uint64_t exterior_layer_allocations;
    uint64_t interior_layer_allocations;
    uint64_t exterior_layer_clears;
    uint64_t interior_layer_clears;
    uint64_t exterior_cleared_pixels;
    uint64_t interior_cleared_pixels;
    uint64_t exterior_retained_bytes;
    uint64_t interior_retained_bytes;
    uint64_t exterior_surface_identity;
    uint64_t interior_surface_identity;
    uint64_t exterior_layer_key;
    uint64_t interior_layer_key;
} OceanDecorationDebugStats;

extern OceanDecorationDebugStats ocean_decoration_debug_stats;

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
    int coverage_rebuilds;
    int coverage_row_spans;
    int coverage_ocean_tiles;
    int coverage_lake_tiles_excluded;
    int coverage_uses_color_key;
    unsigned int item_hash;
    unsigned int motif_mask;
} OceanDecorationProbeInfo;

void render_ocean_decoration_draw_background(HDC hdc, RECT client, MapLayout layout,
                                             const RenderSnapshot *snapshot);
int render_ocean_decoration_prewarm_background(
    HDC hdc, RECT client, MapLayout layout,
    const RenderSnapshot *snapshot);
void render_ocean_decoration_draw(HDC hdc, RECT client, MapLayout layout,
                                  const RenderSnapshot *snapshot);
RenderLayerCacheMemory render_ocean_decoration_memory(void);
void render_ocean_decoration_reset_debug(void);
OceanDecorationProbeInfo render_ocean_decoration_probe_info(void);

#endif
