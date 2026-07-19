#include "game/game_worldgen_live_validation_probe.h"

#include "core/render_snapshot.h"
#include "core/render_snapshot_keys.h"
#include "game/game_worldgen_live_validation.h"

#include <string.h>

static int config_matches(const WorldgenLiveValidationSnapshot *snapshot,
                          const WorldGenConfig *config) {
    return snapshot->worldgen_config_valid && config &&
           snapshot->worldgen_seed == config->seed &&
           snapshot->worldgen_random_seed == 0 &&
           snapshot->config_ocean == config->ocean &&
           snapshot->config_continent == config->continent &&
           snapshot->config_relief == config->relief &&
           snapshot->config_moisture == config->moisture &&
           snapshot->config_drought == config->drought &&
           snapshot->config_vegetation == config->vegetation &&
           snapshot->config_bias_forest == config->bias_forest &&
           snapshot->config_bias_desert == config->bias_desert &&
           snapshot->config_bias_mountain == config->bias_mountain &&
           snapshot->config_bias_wetland == config->bias_wetland;
}

static int semantic_samples_valid(const WorldgenLiveValidationSnapshot *snapshot) {
    static const int required[WORLDGEN_LIVE_SEMANTIC_COUNT] = {
        1, 1, 0, 1, 1, 0, 0
    };
    int semantic;
    for (semantic = 0; semantic < WORLDGEN_LIVE_SEMANTIC_COUNT; semantic++) {
        const WorldgenLiveSemanticSample *sample = &snapshot->semantics[semantic];
        if (sample->count < 0 || (required[semantic] && sample->count == 0)) return 0;
        if (sample->count == 0) {
            if (sample->first_x != -1 || sample->first_y != -1) return 0;
        } else if (sample->first_x < 0 || sample->first_x >= snapshot->map_w ||
                   sample->first_y < 0 || sample->first_y >= snapshot->map_h) return 0;
    }
    return snapshot->semantics[WORLDGEN_LIVE_SEMANTIC_CLOSED_BASIN].count <=
               snapshot->semantics[WORLDGEN_LIVE_SEMANTIC_LAKE].count &&
           snapshot->semantics[WORLDGEN_LIVE_SEMANTIC_SALT_LAKE].count <=
               snapshot->semantics[WORLDGEN_LIVE_SEMANTIC_CLOSED_BASIN].count;
}

int game_worldgen_live_validation_probe_run(FILE *file,
                                            const WorldGenConfig *expected_config) {
    WorldgenLiveValidationSnapshot first;
    WorldgenLiveValidationSnapshot second;
    WorldgenLiveRenderCacheSnapshot cache;
    WorldGenContext *invalid_prepare;
    int abi_ok;
    int revision_ok;
    int hash_ok;
    int diagnostics_ok;
    int semantics_ok;
    int stable;
    int cache_abi_ok;
    int ok;
    if (!file || !expected_config) return 0;
    render_snapshot_publish_from_live_state();
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    memset(&cache, 0, sizeof(cache));
    game_worldgen_live_validation_snapshot(&first);
    invalid_prepare = world_gen_prepare_for_dimensions(expected_config, 0, 0);
    if (invalid_prepare) world_gen_release_prepared(invalid_prepare);
    game_worldgen_live_validation_snapshot(&second);
    game_worldgen_live_render_cache_snapshot(&cache);
    abi_ok = first.magic == WORLDGEN_LIVE_VALIDATION_MAGIC &&
             first.version == WORLDGEN_LIVE_VALIDATION_VERSION &&
             first.struct_size == sizeof(first) && first.snapshot_available &&
             first.world_generated && first.map_w > 0 && first.map_h > 0;
    revision_ok = first.live_terrain_revision == first.snapshot_terrain_revision &&
                  first.live_coast_revision == first.snapshot_coast_revision &&
                  first.live_hydrology_revision == first.snapshot_hydrology_revision &&
                  first.physical_revision > 0 &&
                  first.snapshot_wind_revision == render_snapshot_wind_revision_key() &&
                  first.snapshot_river_revision == render_snapshot_river_revision_key();
    hash_ok = first.physical_valid && first.wind_hash_valid && first.river_hash_valid &&
              first.physical_hash != 0 && first.wind_hash != 0 && first.river_hash != 0 &&
              first.physical_tile_count == first.map_w * first.map_h &&
              first.wind_coarse_count > 0 && first.wind_medium_count > 0 &&
              first.wind_fine_count > 0 && first.river_path_count > 0;
    diagnostics_ok = first.worldgen_diagnostics_valid && config_matches(&first, expected_config) &&
                     first.worldgen_physical_hash != 0 && first.worldgen_total_ms > 0 &&
                     first.worldgen_peak_bytes >= first.worldgen_context_bytes &&
                     first.worldgen_land_tiles + first.worldgen_ocean_tiles ==
                         first.physical_tile_count &&
                     first.worldgen_river_segments_required == first.river_path_count &&
                     first.worldgen_river_segments_copied == first.river_path_count;
    semantics_ok = semantic_samples_valid(&first);
    stable = !invalid_prepare && memcmp(&first, &second, sizeof(first)) == 0;
    cache_abi_ok = cache.magic == WORLDGEN_LIVE_RENDER_CACHE_MAGIC &&
                   cache.version == WORLDGEN_LIVE_RENDER_CACHE_VERSION &&
                   cache.struct_size == sizeof(cache) &&
                   cache.retained_bitmap_bytes == cache.static_map_bitmap_bytes +
                       cache.static_scene_bitmap_bytes + cache.ocean_bitmap_bytes +
                       cache.physical_bitmap_bytes + cache.overlay_bitmap_bytes +
                       cache.wind_sprite_bitmap_bytes +
                       cache.legend_bitmap_bytes &&
                   cache.retained_total_bytes == cache.retained_bitmap_bytes +
                       cache.coast_heap_bytes +
                       cache.ocean_coverage_field_bytes +
                       cache.shore_color_heap_bytes;
    ok = abi_ok && revision_ok && hash_ok && diagnostics_ok && semantics_ok &&
         stable && cache_abi_ok;
    fprintf(file, "case=worldgen_live_validation abi=%d revisions=%d "
                  "live_revision=%d/%d/%d snapshot_revision=%d/%d/%d "
                  "physical_revision=%d wind_revision=%d river_revision=%d hashes=%d "
                  "diagnostics=%d semantics=%d failed_prepare_guard=%d size=%u map=%dx%d "
                  "physical=%016llx wind=%016llx river=%016llx paths=%d "
                  "cache_abi=%d cache_bytes=%llu ok=%d\n",
            abi_ok, revision_ok,
            first.live_terrain_revision, first.live_coast_revision,
            first.live_hydrology_revision, first.snapshot_terrain_revision,
            first.snapshot_coast_revision, first.snapshot_hydrology_revision,
            first.physical_revision, first.snapshot_wind_revision,
            first.snapshot_river_revision, hash_ok, diagnostics_ok, semantics_ok, stable,
            first.struct_size, first.map_w, first.map_h,
            (unsigned long long)first.physical_hash,
            (unsigned long long)first.wind_hash,
            (unsigned long long)first.river_hash, first.river_path_count,
            cache_abi_ok, (unsigned long long)cache.retained_bitmap_bytes, ok);
    return ok;
}
