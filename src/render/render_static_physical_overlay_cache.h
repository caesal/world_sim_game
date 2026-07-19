#ifndef WORLD_SIM_RENDER_STATIC_PHYSICAL_OVERLAY_CACHE_H
#define WORLD_SIM_RENDER_STATIC_PHYSICAL_OVERLAY_CACHE_H

#include <stdint.h>
#include <windows.h>

#include "core/render_snapshot.h"
#include "ui/ui_layout.h"

enum {
    RENDER_STATIC_OVERLAY_RIVER_LOD_COUNT = 4,
    RENDER_STATIC_OVERLAY_WIND_LOD_COUNT = 3
};

typedef struct {
    int prewarm_attempts;
    int prewarm_completions;
    int prewarm_last_ms;
    int river_ensure_calls;
    int river_hits;
    int river_misses;
    int river_rebuilds;
    int river_build_failures;
    int river_evictions;
    uint64_t river_path_visits;
    int wind_ensure_calls;
    int wind_hits;
    int wind_misses;
    int wind_rebuilds;
    int wind_evictions;
    uint64_t wind_sample_visits;
    int river_present_hits;
    int river_present_misses;
    int wind_present_hits;
    int wind_present_misses;
    int invalidations;
    int river_surface_allocations;
    int wind_surface_allocations;
    int wind_sprite_blits;
    uint64_t wind_anchor_visits;
    uint64_t wind_anchor_bytes;
    uint64_t wind_sprite_bytes;
    uint64_t surface_clear_pixels;
    uint64_t surface_finalize_pixels;
    int persistent_bitmaps;
    int persistent_dcs;
    uint64_t persistent_bitmap_bytes;
    int ready_river_entries;
    int ready_wind_entries;
    unsigned int ready_river_mask;
    unsigned int ready_wind_mask;
    int selected_river_lod;
    int selected_river_visible;
    int selected_river_target;
    int selected_river_connected;
    int selected_river_scale;
    int selected_wind_lod;
    int selected_wind_samples;
    int selected_wind_scale;
} RenderStaticPhysicalOverlayCacheStats;

int render_static_physical_overlay_cache_ensure_river(
    HDC hdc, const RenderSnapshot *snapshot, int lod);
int render_static_physical_overlay_cache_ensure_wind(
    HDC hdc, const RenderSnapshot *snapshot, int lod);
int render_static_physical_overlay_cache_prewarm_all(
    HDC hdc, const RenderSnapshot *snapshot);
int render_static_physical_overlay_cache_river_ready(
    const RenderSnapshot *snapshot, int lod);
int render_static_physical_overlay_cache_wind_ready(
    const RenderSnapshot *snapshot, int lod);
unsigned int render_static_physical_overlay_cache_river_ready_mask(
    const RenderSnapshot *snapshot);
unsigned int render_static_physical_overlay_cache_wind_ready_mask(
    const RenderSnapshot *snapshot);
void render_static_physical_overlay_cache_present_river(
    HDC destination, RECT client, MapLayout layout,
    const RenderSnapshot *snapshot, int lod);
void render_static_physical_overlay_cache_present_wind(
    HDC destination, RECT client, MapLayout layout,
    const RenderSnapshot *snapshot, int lod);
const RenderStaticPhysicalOverlayCacheStats *
render_static_physical_overlay_cache_stats(void);
void render_static_physical_overlay_cache_reset_debug_counters(void);
void render_static_physical_overlay_cache_invalidate(void);

#endif
