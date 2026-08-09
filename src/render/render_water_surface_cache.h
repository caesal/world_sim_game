#ifndef WORLD_SIM_RENDER_WATER_SURFACE_CACHE_H
#define WORLD_SIM_RENDER_WATER_SURFACE_CACHE_H

#include "core/render_snapshot.h"
#include "render/render_layer_cache.h"
#include "ui/ui_types.h"

#include <stdint.h>
#include <windows.h>

typedef struct {
    int rebuilds;
    int cache_hits;
    int presents;
    int lake_presents;
    int ocean_presents;
    int texture_score;
    int interior_water_only;
    int interior_deep_only;
    int interior_shallow_allowed_seen;
    int interior_min_clearance;
    int ocean_tiles;
    int lake_tiles;
    int water_row_spans;
    uint64_t mask_pixel_scans;
    uint64_t ocean_pixels;
    uint64_t lake_pixels;
    int texture_inset_radius;
    uint64_t ocean_partial_coverage_pixels;
    uint64_t lake_partial_coverage_pixels;
    uint64_t coast_thin_components;
    uint64_t coast_one_ended_components;
    uint64_t coast_preserved_components;
    uint64_t coast_sparse_mesh_components;
    uint64_t coast_sparse_mesh_tiles;
    uint64_t coast_regularized_components;
    uint64_t coast_regularized_tiles;
    uint64_t coast_regularized_pixels;
    uint64_t coast_removed_ocean_tiles;
    uint64_t coast_filled_land_tiles;
    uint64_t coast_removed_ocean_pixels;
    uint64_t coast_filled_land_pixels;
    uint64_t coast_protected_land_tiles;
    uint64_t coast_protected_marine_tiles;
    uint64_t coast_protected_marine_components;
    uint64_t coast_cleanup_tiles;
    uint64_t coast_presentation_hash;
    uint64_t coast_candidate_comparisons;
    uint64_t coast_smoothing_transient_bytes;
    int fallback_lake_draws;
    int fallback_ocean_draws;
    uint64_t fallback_tile_scans;
    int persistent_bitmaps;
    int persistent_dcs;
    uint64_t persistent_bitmap_bytes;
} RenderWaterSurfaceCacheStats;

extern RenderWaterSurfaceCacheStats water_surface_cache_debug_stats;

uint32_t render_water_surface_lake_tint_pixel(uint32_t source_pixel);
int render_water_surface_cache_pixel_is_lake(
    const RenderSnapshot *snapshot, MapLayout layout, int px, int py);
int render_water_surface_cache_ensure(HDC hdc,
                                      const RenderSnapshot *snapshot);
int render_water_surface_cache_ready(const RenderSnapshot *snapshot);
void render_water_surface_cache_present_lake(HDC hdc, RECT client,
                                             MapLayout layout,
                                             const RenderSnapshot *snapshot);
void render_water_surface_cache_present_ocean(HDC hdc, RECT client,
                                              MapLayout layout,
                                              const RenderSnapshot *snapshot);
void render_water_surface_cache_present(HDC hdc, RECT client,
                                        MapLayout layout,
                                        const RenderSnapshot *snapshot);
RenderLayerCacheMemory render_water_surface_cache_memory(void);
const RenderWaterSurfaceCacheStats *render_water_surface_cache_stats(void);
void render_water_surface_cache_invalidate(void);
void render_water_surface_cache_reset_debug(void);

#endif
