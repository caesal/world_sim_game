#include "game/game_worldgen_live_validation.h"

#include "core/dirty_flags.h"
#include "core/profiler.h"
#include "core/render_snapshot.h"
#include "render/river_geometry.h"
#include "render/coast_geometry.h"
#include "render/map_shore_color_cache.h"
#include "render/panel_map_water_legend.h"
#include "render/render_allocation_diagnostics.h"
#include "render/render_ocean_coverage.h"
#include "render/render_ocean_decoration.h"
#include "render/render_static_map_cache.h"
#include "render/render_static_physical_cache.h"
#include "render/render_static_physical_overlay_cache.h"
#include "render/render_static_scene.h"
#include "render/render_water_coverage.h"
#include "render/render_water_surface_cache.h"
#include "render/render_world_static_prewarm.h"
#include "render/wind_render.h"
#include "world/river_path_validation.h"
#include "world/world_gen.h"
#include "world/world_physical_state.h"

#include <string.h>

static uint64_t hash_value(uint64_t hash, uint64_t value) {
    int byte_index;
    for (byte_index = 0; byte_index < 8; byte_index++) {
        hash ^= (uint8_t)(value >> (byte_index * 8));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int snapshot_dimensions_valid(const RenderSnapshot *snapshot) {
    return snapshot && snapshot->map_w > 0 && snapshot->map_w <= MAX_MAP_W &&
           snapshot->map_h > 0 && snapshot->map_h <= MAX_MAP_H;
}

static uint64_t physical_snapshot_hash(const RenderSnapshot *snapshot, int *valid) {
    const WorldPhysicalTileState *physical = world_physical_state_tiles();
    uint64_t hash = UINT64_C(1469598103934665603);
    int tile_count;
    int i;
    *valid = snapshot_dimensions_valid(snapshot) && world_physical_state_valid() && physical &&
             world_physical_state_width() == snapshot->map_w &&
             world_physical_state_height() == snapshot->map_h;
    if (!*valid) return 0;
    tile_count = snapshot->map_w * snapshot->map_h;
    if (world_physical_state_tile_count() != tile_count) {
        *valid = 0;
        return 0;
    }
    hash = hash_value(hash, (uint32_t)snapshot->map_w);
    hash = hash_value(hash, (uint32_t)snapshot->map_h);
    for (i = 0; i < tile_count; i++) {
        const SnapshotTile *tile = &snapshot->tiles[i];
        const WorldPhysicalTileState *state = &physical[i];
        uint64_t labels = tile->geography |
                          ((uint64_t)tile->climate << 8) |
                          ((uint64_t)tile->ecology << 16) |
                          ((uint64_t)tile->resource << 24) |
                          ((uint64_t)tile->river << 32) |
                          ((uint64_t)tile->elevation << 40) |
                          ((uint64_t)tile->water_depth << 48) |
                          ((uint64_t)tile->water_deep_percent << 56);
        uint64_t climate = (uint16_t)tile->moisture |
                           ((uint64_t)(uint16_t)tile->temperature << 16);
        uint64_t hydro_wind = state->river_flow |
                              ((uint64_t)state->river_width << 32) |
                              ((uint64_t)state->wind_direction16 << 48) |
                              ((uint64_t)state->wind_speed << 56);
        uint64_t soil_flags = state->soil_fertility |
                              ((uint64_t)state->river_order << 8) |
                              ((uint64_t)state->river_flags << 16);
        hash = hash_value(hash, labels);
        hash = hash_value(hash, climate);
        hash = hash_value(hash, hydro_wind);
        hash = hash_value(hash, soil_flags);
    }
    return hash;
}

static uint64_t hash_wind_samples(uint64_t hash, const SnapshotWindSample *samples,
                                  int count) {
    int i;
    for (i = 0; i < count; i++) {
        uint64_t value = samples[i].x |
                         ((uint64_t)samples[i].y << 16) |
                         ((uint64_t)samples[i].direction << 32) |
                         ((uint64_t)samples[i].speed << 40);
        hash = hash_value(hash, value);
    }
    return hash;
}

static uint64_t wind_snapshot_hash(const RenderSnapshot *snapshot, int *valid) {
    const SnapshotWindField *wind;
    uint64_t hash = UINT64_C(1469598103934665603);
    *valid = snapshot_dimensions_valid(snapshot) && snapshot->wind.valid;
    if (!*valid) return 0;
    wind = &snapshot->wind;
    if (wind->map_w != snapshot->map_w || wind->map_h != snapshot->map_h ||
        wind->coarse_count < 0 || wind->coarse_count > SNAPSHOT_WIND_COARSE_MAX ||
        wind->medium_count < 0 || wind->medium_count > SNAPSHOT_WIND_MEDIUM_MAX ||
        wind->fine_count < 0 || wind->fine_count > SNAPSHOT_WIND_FINE_MAX) {
        *valid = 0;
        return 0;
    }
    hash = hash_value(hash, (uint32_t)wind->map_w);
    hash = hash_value(hash, (uint32_t)wind->map_h);
    hash = hash_value(hash, (uint32_t)wind->coarse_count);
    hash = hash_value(hash, (uint32_t)wind->medium_count);
    hash = hash_value(hash, (uint32_t)wind->fine_count);
    hash = hash_wind_samples(hash, wind->coarse, wind->coarse_count);
    hash = hash_wind_samples(hash, wind->medium, wind->medium_count);
    return hash_wind_samples(hash, wind->fine, wind->fine_count);
}

static uint64_t river_snapshot_hash(const RenderSnapshot *snapshot, int *valid) {
    const SnapshotRiverField *rivers;
    uint64_t hash = UINT64_C(1469598103934665603);
    int i;
    *valid = snapshot_dimensions_valid(snapshot) && snapshot->rivers.valid;
    if (!*valid) return 0;
    rivers = &snapshot->rivers;
    if (rivers->map_w != snapshot->map_w || rivers->map_h != snapshot->map_h ||
        !river_path_count_valid(rivers->path_count,
                                snapshot->map_w, snapshot->map_h) ||
        rivers->capacity != rivers->path_count ||
        (rivers->path_count > 0 && !rivers->paths) ||
        (rivers->path_count == 0 && rivers->paths)) {
        *valid = 0;
        return 0;
    }
    hash = hash_value(hash, (uint32_t)rivers->map_w);
    hash = hash_value(hash, (uint32_t)rivers->map_h);
    hash = hash_value(hash, (uint32_t)rivers->path_count);
    for (i = 0; i < rivers->path_count; i++) {
        const SnapshotRiverPath *path = &rivers->paths[i];
        uint64_t metadata;
        int point_index;
        if (path->point_count < 2 || path->point_count > MAX_RIVER_POINTS) {
            *valid = 0;
            return 0;
        }
        metadata = path->flow |
                   ((uint64_t)path->point_count << 32) |
                   ((uint64_t)path->width << 48);
        hash = hash_value(hash, metadata);
        hash = hash_value(hash, path->order |
                                ((uint64_t)path->semantic_flags << 8) |
                                ((uint64_t)path->end_flags << 16));
        for (point_index = 0; point_index < path->point_count; point_index++) {
            const SnapshotRiverPoint *point = &path->points[point_index];
            if (point->x >= snapshot->map_w || point->y >= snapshot->map_h) {
                *valid = 0;
                return 0;
            }
            hash = hash_value(hash, point->x |
                                    ((uint64_t)point->y << 16) |
                                    ((uint64_t)point->semantic_flags << 32));
        }
    }
    return hash;
}

static void collect_semantics(WorldgenLiveValidationSnapshot *out) {
    static const uint16_t flags[WORLDGEN_LIVE_SEMANTIC_COUNT] = {
        WORLD_RIVER_TILE_SOURCE, WORLD_RIVER_TILE_CONFLUENCE, WORLD_RIVER_TILE_LAKE,
        WORLD_RIVER_TILE_MOUTH, WORLD_RIVER_TILE_DELTA, WORLD_RIVER_TILE_CLOSED_BASIN,
        WORLD_RIVER_TILE_SALT_LAKE
    };
    const WorldPhysicalTileState *tiles = world_physical_state_tiles();
    int width = world_physical_state_width();
    int count = world_physical_state_tile_count();
    int i;
    int semantic;
    for (semantic = 0; semantic < WORLDGEN_LIVE_SEMANTIC_COUNT; semantic++) {
        out->semantics[semantic].first_x = -1;
        out->semantics[semantic].first_y = -1;
    }
    if (!out->physical_valid || !tiles || width <= 0) return;
    for (i = 0; i < count; i++) {
        for (semantic = 0; semantic < WORLDGEN_LIVE_SEMANTIC_COUNT; semantic++) {
            WorldgenLiveSemanticSample *sample = &out->semantics[semantic];
            if (!(tiles[i].river_flags & flags[semantic])) continue;
            sample->count++;
            if (sample->first_x < 0) {
                sample->first_x = i % width;
                sample->first_y = i / width;
            }
        }
    }
}

static void copy_render_stats(WorldgenLiveValidationSnapshot *out) {
    const WindRenderStats *wind = wind_render_stats();
    const HydrologyRenderStats *river = river_geometry_stats();
    RuntimeProfilerSnapshot profiler;
    memset(&profiler, 0, sizeof(profiler));
    profiler_snapshot(&profiler);
    out->profiler_terrain_rebuild_count = profiler.terrain_rebuild_count;
    if (wind) {
        out->wind_geometry_rebuild_count = wind->geometry_rebuild_count;
        out->wind_geometry_reuse_count = wind->geometry_reuse_count;
        out->wind_draw_count = wind->draw_count;
        out->wind_last_sample_count = wind->last_sample_count;
        out->wind_last_lod = wind->last_lod;
        out->wind_last_revision = wind->last_revision;
    }
    if (!river) return;
    out->river_count = river->river_count;
    out->river_average_length = river->average_length;
    out->river_longest_length = river->longest_length;
    out->river_main_rivers = river->main_rivers;
    out->river_tributaries = river->tributaries;
    out->river_confluences = river->confluences;
    out->river_invalid_uphill_segments = river->invalid_uphill_segments;
    out->river_inland_dead_ends = river->inland_dead_ends;
    out->river_overly_short_rivers = river->overly_short_rivers;
    out->river_cache_rebuild_count = river->cache_rebuild_count;
    out->river_cache_rebuild_ms = river->cache_rebuild_ms;
    out->river_geometry_rebuild_count = river->geometry_rebuild_count;
    out->river_geometry_rebuild_ms = river->geometry_rebuild_ms;
    out->river_visible_count_last_draw = river->visible_river_count_last_draw;
    out->river_skipped_by_lod_last_draw = river->skipped_by_lod_last_draw;
}

static void copy_worldgen_diagnostics(WorldgenLiveValidationSnapshot *out) {
    WorldGenDiagnostics diagnostics;
    WorldGenConfig config;
    memset(&diagnostics, 0, sizeof(diagnostics));
    memset(&config, 0, sizeof(config));
    out->worldgen_diagnostics_valid = world_gen_last_committed_diagnostics(&diagnostics);
    if (out->worldgen_diagnostics_valid) {
        out->worldgen_elevation_ms = diagnostics.elevation_ms;
        out->worldgen_mountain_ms = diagnostics.mountain_ms;
        out->worldgen_climate_ms = diagnostics.climate_ms;
        out->worldgen_hydrology_ms = diagnostics.hydrology_ms;
        out->worldgen_classification_ms = diagnostics.classification_ms;
        out->worldgen_commit_ms = diagnostics.commit_ms;
        out->worldgen_total_ms = diagnostics.total_ms;
        out->worldgen_context_bytes = diagnostics.context_bytes;
        out->worldgen_hydrology_bytes = diagnostics.hydrology_bytes;
        out->worldgen_staged_path_bytes = diagnostics.staged_path_bytes;
        out->worldgen_peak_bytes = diagnostics.peak_bytes;
        out->worldgen_physical_hash = diagnostics.physical_hash;
        out->worldgen_land_tiles = diagnostics.land_tiles;
        out->worldgen_ocean_tiles = diagnostics.ocean_tiles;
        out->worldgen_mountain_tiles = diagnostics.mountain_tiles;
        out->worldgen_river_tiles = diagnostics.river_tiles;
        out->worldgen_river_segments_required = diagnostics.river_segments_required;
        out->worldgen_river_segments_copied = diagnostics.river_segments_copied;
    }
    out->worldgen_config_valid = world_gen_last_committed_config(&config);
    if (!out->worldgen_config_valid) return;
    out->worldgen_seed = config.seed;
    out->worldgen_random_seed = config.random_seed;
    out->config_ocean = config.ocean;
    out->config_continent = config.continent;
    out->config_relief = config.relief;
    out->config_moisture = config.moisture;
    out->config_drought = config.drought;
    out->config_vegetation = config.vegetation;
    out->config_bias_forest = config.bias_forest;
    out->config_bias_desert = config.bias_desert;
    out->config_bias_mountain = config.bias_mountain;
    out->config_bias_wetland = config.bias_wetland;
}

void game_worldgen_live_validation_snapshot(void *output) {
    WorldgenLiveValidationSnapshot out;
    const RenderSnapshot *snapshot;
    if (!output) return;
    memset(&out, 0, sizeof(out));
    out.magic = WORLDGEN_LIVE_VALIDATION_MAGIC;
    out.version = WORLDGEN_LIVE_VALIDATION_VERSION;
    out.struct_size = (uint32_t)sizeof(out);
    out.live_terrain_revision = dirty_revision_terrain();
    out.live_coast_revision = dirty_revision_coast();
    out.live_hydrology_revision = dirty_revision_hydrology();
    out.physical_revision = world_physical_state_revision();
    out.physical_tile_count = world_physical_state_tile_count();
    snapshot = render_snapshot_acquire();
    out.snapshot_available = snapshot != NULL;
    if (snapshot) {
        out.snapshot_revision = snapshot->revision;
        out.map_w = snapshot->map_w;
        out.map_h = snapshot->map_h;
        out.year = snapshot->year;
        out.month = snapshot->month;
        out.world_generated = snapshot->world_generated;
        out.snapshot_terrain_revision = snapshot->terrain_revision;
        out.snapshot_coast_revision = snapshot->coast_revision;
        out.snapshot_hydrology_revision = snapshot->hydrology_revision;
        out.snapshot_wind_revision = snapshot->wind_revision;
        out.snapshot_river_revision = snapshot->river_revision;
        out.wind_coarse_count = snapshot->wind.coarse_count;
        out.wind_medium_count = snapshot->wind.medium_count;
        out.wind_fine_count = snapshot->wind.fine_count;
        out.river_path_count = snapshot->rivers.path_count;
        out.physical_hash = physical_snapshot_hash(snapshot, &out.physical_valid);
        out.wind_hash = wind_snapshot_hash(snapshot, &out.wind_hash_valid);
        out.river_hash = river_snapshot_hash(snapshot, &out.river_hash_valid);
    }
    render_snapshot_release(snapshot);
    collect_semantics(&out);
    copy_render_stats(&out);
    copy_worldgen_diagnostics(&out);
    memcpy(output, &out, sizeof(out));
}

static void add_memory(RenderLayerCacheMemory *total,
                       RenderLayerCacheMemory part) {
    total->bitmaps += part.bitmaps;
    total->dcs += part.dcs;
    total->bitmap_bytes += part.bitmap_bytes;
}

void game_worldgen_live_render_cache_snapshot(void *output) {
    WorldgenLiveRenderCacheSnapshot out;
    RenderLayerCacheMemory total = {0};
    RenderLayerCacheMemory static_map = render_static_map_cache_memory();
    RenderLayerCacheMemory scene = render_static_scene_memory();
    RenderLayerCacheMemory ocean = render_ocean_decoration_memory();
    const RenderStaticPhysicalCacheStats *physical =
        render_static_physical_cache_stats();
    const RenderStaticPhysicalOverlayCacheStats *overlay =
        render_static_physical_overlay_cache_stats();
    const CoastGeometryStats *coast = coast_geometry_stats();
    const RenderOceanCoverageStats *coverage = render_ocean_coverage_stats();
    const OceanDecorationProbeInfo ocean_probe =
        render_ocean_decoration_probe_info();
    const RenderWorldStaticPrewarmStats *prewarm =
        render_world_static_prewarm_stats();
    const RenderWaterSurfaceCacheStats *water =
        render_water_surface_cache_stats();
    const RenderWaterCoverageStats *water_coverage =
        render_water_coverage_stats();
    const MapShoreColorCacheStats *shore = map_shore_color_cache_stats();
    const PanelMapWaterLegendPatternStats *legend =
        panel_map_water_legend_pattern_stats();
    RenderAllocationDiagnostics allocation;
    int i;
    if (!output) return;
    memset(&out, 0, sizeof(out));
    memset(&allocation, 0, sizeof(allocation));
    render_allocation_diagnostics_get(&allocation);
    add_memory(&total, static_map);
    add_memory(&total, scene);
    add_memory(&total, ocean);
    out.physical_bitmap_bytes = physical->persistent_bitmap_bytes;
    out.overlay_bitmap_bytes = overlay->persistent_bitmap_bytes;
    out.wind_sprite_bitmap_bytes = overlay->wind_sprite_bytes;
    out.legend_bitmap_bytes = legend->ready ?
        (uint64_t)legend->width * (uint64_t)legend->height * 4u : 0u;
    out.wind_sprite_bitmaps = out.wind_sprite_bitmap_bytes > 0;
    out.wind_sprite_dcs = out.wind_sprite_bitmaps;
    out.legend_bitmaps = out.legend_bitmap_bytes > 0;
    out.legend_dcs = out.legend_bitmaps;
    out.retained_bitmap_bytes = total.bitmap_bytes +
        out.physical_bitmap_bytes + out.overlay_bitmap_bytes +
        out.wind_sprite_bitmap_bytes + out.legend_bitmap_bytes;
    out.static_map_bitmap_bytes = static_map.bitmap_bytes;
    out.static_scene_bitmap_bytes = scene.bitmap_bytes;
    out.ocean_bitmap_bytes = ocean.bitmap_bytes;
    out.coast_heap_bytes = coast->retained_bytes;
    out.ocean_coverage_field_bytes = coverage->field_retained_bytes;
    out.retained_total_bytes = out.retained_bitmap_bytes +
        out.coast_heap_bytes + out.ocean_coverage_field_bytes +
        shore->retained_bytes;
    out.retained_bitmaps = total.bitmaps + physical->persistent_bitmaps +
                           overlay->persistent_bitmaps +
                           out.wind_sprite_bitmaps + out.legend_bitmaps;
    out.retained_dcs = total.dcs + physical->persistent_dcs +
                       overlay->persistent_dcs + out.wind_sprite_dcs +
                       out.legend_dcs;
    for (i = 0; i < RENDER_ALLOCATION_COUNT; i++) {
        out.allocation_attempts += allocation.attempts[i];
        out.allocation_failures += allocation.failures[i];
    }
    out.injected_allocation_failures = allocation.injected_failures;
    out.allocation_last_failure_owner = allocation.last_failure_owner;
    out.allocation_last_failure_bytes = allocation.last_failure_bytes;
    out.allocation_last_failure_width = allocation.last_failure_width;
    out.allocation_last_failure_height = allocation.last_failure_height;
    out.snapshot_fallback_draws = render_static_map_cache_snapshot_fallback_draws();
    out.physical_base_rebuilds = physical->base_rebuilds[0] +
        physical->base_rebuilds[1] + physical->base_rebuilds[2];
    out.physical_coast_rebuilds = physical->coast_rebuilds;
    out.river_overlay_rebuilds = overlay->river_rebuilds;
    out.wind_overlay_rebuilds = overlay->wind_rebuilds;
    out.coast_geometry_rebuilds = coast->rebuilds;
    out.coast_geometry_draws = coast->draws;
    out.coast_segment_count = coast->segment_count;
    out.coast_mixed_cell_count = coast->mixed_cell_count;
    out.coast_segment_visits = coast->segment_visits;
    out.coast_cell_visits = coast->mixed_cell_visits;
    out.ocean_exterior_rebuilds = ocean_probe.exterior_rebuilds;
    out.ocean_interior_rebuilds = ocean_probe.interior_rebuilds;
    out.ocean_coverage_rebuilds = coverage->field_rebuilds;
    out.ocean_coverage_hits = 0;
    out.ocean_coverage_spans = 0;
    out.scene_cache_hits = render_scene_cache_hits();
    out.scene_cache_misses = render_scene_cache_misses();
    out.scene_viewport_rebuilds = render_scene_cache_viewport_rebuilds();
    out.static_prewarm_attempts = prewarm->attempts;
    out.static_prewarm_completions = prewarm->completions;
    out.static_prewarm_failures = prewarm->failures;
    for (i = 0; i < MAP_PHYSICAL_BASE_COUNT; i++)
        out.physical_base_tile_scans += physical->base_tile_scans[i];
    out.physical_coast_tile_scans = physical->coast_tile_scans;
    out.river_path_visits = overlay->river_path_visits;
    out.wind_sample_visits = overlay->wind_sample_visits;
    out.overlay_surface_clear_pixels = overlay->surface_clear_pixels;
    out.water_mask_pixel_scans = water->mask_pixel_scans;
    out.water_coverage_source_scans = water_coverage->source_scans;
    out.water_coverage_raster_samples = water_coverage->raster_samples;
    out.water_fallback_tile_scans = water->fallback_tile_scans;
    out.water_surface_rebuilds = water->rebuilds;
    out.water_coverage_rebuilds = (int32_t)water_coverage->rebuilds;
    out.water_fallback_lake_draws = water->fallback_lake_draws;
    out.water_fallback_ocean_draws = water->fallback_ocean_draws;
    out.river_surface_allocations = overlay->river_surface_allocations;
    out.wind_surface_allocations = overlay->wind_surface_allocations;
    out.static_map_compositions = render_static_map_cache_compositions();
    out.shore_color_tile_scans = shore->tile_scans;
    out.shore_color_heap_bytes = shore->retained_bytes;
    out.shore_color_rebuilds = (int32_t)shore->rebuilds;
    out.magic = WORLDGEN_LIVE_RENDER_CACHE_MAGIC;
    out.version = WORLDGEN_LIVE_RENDER_CACHE_VERSION;
    out.struct_size = (uint32_t)sizeof(out);
    memcpy(output, &out, sizeof(out));
}
