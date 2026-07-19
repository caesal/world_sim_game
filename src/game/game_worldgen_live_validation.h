#ifndef WORLD_SIM_GAME_WORLDGEN_LIVE_VALIDATION_H
#define WORLD_SIM_GAME_WORLDGEN_LIVE_VALIDATION_H

#include <stdint.h>

enum {
    WORLDGEN_LIVE_VALIDATION_MAGIC = 0x31564757u,
    WORLDGEN_LIVE_VALIDATION_VERSION = 1,
    WORLDGEN_LIVE_SEMANTIC_SOURCE = 0,
    WORLDGEN_LIVE_SEMANTIC_CONFLUENCE,
    WORLDGEN_LIVE_SEMANTIC_LAKE,
    WORLDGEN_LIVE_SEMANTIC_MOUTH,
    WORLDGEN_LIVE_SEMANTIC_DELTA,
    WORLDGEN_LIVE_SEMANTIC_CLOSED_BASIN,
    WORLDGEN_LIVE_SEMANTIC_SALT_LAKE,
    WORLDGEN_LIVE_SEMANTIC_COUNT
};

enum {
    WORLDGEN_LIVE_RENDER_CACHE_MAGIC = 0x31434757u,
    WORLDGEN_LIVE_RENDER_CACHE_VERSION = 4
};

typedef struct {
    int32_t count;
    int32_t first_x;
    int32_t first_y;
} WorldgenLiveSemanticSample;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t struct_size;
    uint32_t snapshot_revision;
    int32_t snapshot_available;
    int32_t map_w;
    int32_t map_h;
    int32_t year;
    int32_t month;
    int32_t world_generated;
    int32_t live_terrain_revision;
    int32_t live_coast_revision;
    int32_t live_hydrology_revision;
    int32_t physical_revision;
    int32_t snapshot_terrain_revision;
    int32_t snapshot_coast_revision;
    int32_t snapshot_hydrology_revision;
    int32_t snapshot_wind_revision;
    int32_t snapshot_river_revision;
    int32_t physical_valid;
    int32_t wind_hash_valid;
    int32_t river_hash_valid;
    uint64_t physical_hash;
    uint64_t wind_hash;
    uint64_t river_hash;
    int32_t physical_tile_count;
    int32_t wind_coarse_count;
    int32_t wind_medium_count;
    int32_t wind_fine_count;
    int32_t river_path_count;
    int32_t wind_geometry_rebuild_count;
    int32_t wind_geometry_reuse_count;
    int32_t wind_draw_count;
    int32_t wind_last_sample_count;
    int32_t wind_last_lod;
    int32_t wind_last_revision;
    int32_t profiler_terrain_rebuild_count;
    int32_t river_count;
    int32_t river_average_length;
    int32_t river_longest_length;
    int32_t river_main_rivers;
    int32_t river_tributaries;
    int32_t river_confluences;
    int32_t river_invalid_uphill_segments;
    int32_t river_inland_dead_ends;
    int32_t river_overly_short_rivers;
    int32_t river_cache_rebuild_count;
    int32_t river_cache_rebuild_ms;
    int32_t river_geometry_rebuild_count;
    int32_t river_geometry_rebuild_ms;
    int32_t river_visible_count_last_draw;
    int32_t river_skipped_by_lod_last_draw;
    int32_t worldgen_diagnostics_valid;
    int32_t worldgen_config_valid;
    uint32_t worldgen_seed;
    int32_t worldgen_random_seed;
    int32_t config_ocean;
    int32_t config_continent;
    int32_t config_relief;
    int32_t config_moisture;
    int32_t config_drought;
    int32_t config_vegetation;
    int32_t config_bias_forest;
    int32_t config_bias_desert;
    int32_t config_bias_mountain;
    int32_t config_bias_wetland;
    uint64_t worldgen_elevation_ms;
    uint64_t worldgen_mountain_ms;
    uint64_t worldgen_climate_ms;
    uint64_t worldgen_hydrology_ms;
    uint64_t worldgen_classification_ms;
    uint64_t worldgen_commit_ms;
    uint64_t worldgen_total_ms;
    uint64_t worldgen_context_bytes;
    uint64_t worldgen_hydrology_bytes;
    uint64_t worldgen_staged_path_bytes;
    uint64_t worldgen_peak_bytes;
    uint64_t worldgen_physical_hash;
    int32_t worldgen_land_tiles;
    int32_t worldgen_ocean_tiles;
    int32_t worldgen_mountain_tiles;
    int32_t worldgen_river_tiles;
    int32_t worldgen_river_segments_required;
    int32_t worldgen_river_segments_copied;
    WorldgenLiveSemanticSample semantics[WORLDGEN_LIVE_SEMANTIC_COUNT];
} WorldgenLiveValidationSnapshot;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t struct_size;
    uint32_t reserved;
    uint64_t retained_bitmap_bytes;
    uint64_t static_map_bitmap_bytes;
    uint64_t static_scene_bitmap_bytes;
    uint64_t ocean_bitmap_bytes;
    uint64_t physical_bitmap_bytes;
    uint64_t overlay_bitmap_bytes;
    uint64_t coast_heap_bytes;
    uint64_t ocean_coverage_field_bytes;
    uint64_t retained_total_bytes;
    uint64_t allocation_attempts;
    uint64_t allocation_failures;
    uint64_t injected_allocation_failures;
    uint64_t allocation_last_failure_bytes;
    uint64_t coast_segment_visits;
    uint64_t coast_cell_visits;
    int32_t retained_bitmaps;
    int32_t retained_dcs;
    int32_t allocation_last_failure_owner;
    int32_t allocation_last_failure_width;
    int32_t allocation_last_failure_height;
    int32_t snapshot_fallback_draws;
    int32_t physical_base_rebuilds;
    int32_t physical_coast_rebuilds;
    int32_t river_overlay_rebuilds;
    int32_t wind_overlay_rebuilds;
    int32_t coast_geometry_rebuilds;
    int32_t coast_geometry_draws;
    int32_t coast_segment_count;
    int32_t coast_mixed_cell_count;
    int32_t ocean_exterior_rebuilds;
    int32_t ocean_interior_rebuilds;
    int32_t ocean_coverage_rebuilds;
    int32_t ocean_coverage_hits;
    int32_t ocean_coverage_spans;
    int32_t scene_cache_hits;
    int32_t scene_cache_misses;
    int32_t scene_viewport_rebuilds;
    int32_t static_prewarm_attempts;
    int32_t static_prewarm_completions;
    int32_t static_prewarm_failures;
    uint64_t physical_base_tile_scans;
    uint64_t physical_coast_tile_scans;
    uint64_t river_path_visits;
    uint64_t wind_sample_visits;
    uint64_t overlay_surface_clear_pixels;
    uint64_t water_mask_pixel_scans;
    uint64_t water_coverage_source_scans;
    uint64_t water_coverage_raster_samples;
    uint64_t water_fallback_tile_scans;
    int32_t water_surface_rebuilds;
    int32_t water_coverage_rebuilds;
    int32_t water_fallback_lake_draws;
    int32_t water_fallback_ocean_draws;
    int32_t river_surface_allocations;
    int32_t wind_surface_allocations;
    int32_t static_map_compositions;
    uint64_t shore_color_tile_scans;
    uint64_t shore_color_heap_bytes;
    int32_t shore_color_rebuilds;
    uint64_t wind_sprite_bitmap_bytes;
    uint64_t legend_bitmap_bytes;
    int32_t wind_sprite_bitmaps;
    int32_t wind_sprite_dcs;
    int32_t legend_bitmaps;
    int32_t legend_dcs;
} WorldgenLiveRenderCacheSnapshot;

void game_worldgen_live_validation_snapshot(void *output);
void game_worldgen_live_render_cache_snapshot(void *output);

#endif
