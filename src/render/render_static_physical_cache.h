#ifndef WORLD_SIM_RENDER_STATIC_PHYSICAL_CACHE_H
#define WORLD_SIM_RENDER_STATIC_PHYSICAL_CACHE_H

#include <stdint.h>
#include <windows.h>

#include "core/render_snapshot.h"
#include "render/map_display_policy.h"
#include "ui/ui_layout.h"

typedef struct {
    int prewarm_attempts;
    int prewarm_completions;
    int prewarm_last_ms;
    int base_rebuilds[MAP_PHYSICAL_BASE_COUNT];
    uint64_t base_tile_scans[MAP_PHYSICAL_BASE_COUNT];
    uint64_t coast_regularized_tiles;
    uint64_t coast_removed_ocean_tiles;
    uint64_t coast_filled_land_tiles;
    uint64_t coast_protected_land_tiles;
    uint64_t coast_protected_marine_tiles;
    uint64_t coast_protected_marine_components;
    uint64_t coast_cleanup_tiles;
    uint64_t coast_presentation_hash;
    uint64_t coast_regularization_transient_bytes;
    int coast_rebuilds;
    uint64_t coast_tile_scans;
    int persistent_bitmaps;
    int persistent_dcs;
    uint64_t persistent_bitmap_bytes;
    unsigned int ready_base_mask;
    int coast_ready;
} RenderStaticPhysicalCacheStats;

int render_static_physical_cache_prewarm(HDC hdc, const RenderSnapshot *snapshot);
int render_static_physical_cache_ensure_selected(HDC hdc, const RenderSnapshot *snapshot,
                                                 int display_mode, int river_lod,
                                                 int wind_lod);
int render_static_physical_cache_compose_base_coast(HDC destination, int display_mode);
int render_static_physical_cache_width(void);
int render_static_physical_cache_height(void);
int render_static_physical_cache_selected_ready(const RenderSnapshot *snapshot,
                                                int display_mode, int river_lod,
                                                int wind_lod);
const RenderStaticPhysicalCacheStats *render_static_physical_cache_stats(void);
void render_static_physical_cache_reset_debug_counters(void);
void render_static_physical_cache_invalidate(void);

#endif
