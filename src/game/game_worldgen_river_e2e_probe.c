#include "game/game_worldgen_river_e2e_probe.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "core/render_snapshot.h"
#include "io/map_save_river_paths.h"
#include "io/map_save_world_physical.h"
#include "render/render_static_physical_overlay_cache.h"
#include "render/river_geometry.h"
#include "render/river_lod_policy.h"
#include "render/river_render.h"
#include "world/river_path_validation.h"
#include "world/river_presentation_state.h"
#include "world/rivers.h"
#include "world/world_gen.h"
#include "world/world_gen_context.h"
#include "world/world_physical_state.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

enum {
    RIVER_E2E_PRIOR_W = 96,
    RIVER_E2E_PRIOR_H = 64,
    RIVER_E2E_FORMER_PATH_CAP = 12288,
    RIVER_E2E_REFERENCE_PATHS = 14265
};

static uint64_t hash_bytes(const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t i;
    for (i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t hash_value(uint64_t hash, uint64_t value) {
    int byte_index;
    for (byte_index = 0; byte_index < 8; byte_index++) {
        hash ^= (unsigned char)(value >> (byte_index * 8));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t physical_hash(void) {
    const WorldPhysicalTileState *tiles = world_physical_state_tiles();
    int count = world_physical_state_tile_count();
    uint64_t hash = UINT64_C(1469598103934665603);
    int i;
    if (!tiles || count <= 0) return 0;
    hash = hash_value(hash, (uint64_t)world_physical_state_width());
    hash = hash_value(hash, (uint64_t)world_physical_state_height());
    hash = hash_value(hash, (uint64_t)count);
    for (i = 0; i < count; i++) {
        hash = hash_value(hash, tiles[i].river_flow);
        hash = hash_value(hash, tiles[i].river_width);
        hash = hash_value(hash, tiles[i].wind_direction16);
        hash = hash_value(hash, tiles[i].wind_speed);
        hash = hash_value(hash, tiles[i].soil_fertility);
        hash = hash_value(hash, tiles[i].river_order);
        hash = hash_value(hash, tiles[i].river_flags);
    }
    return hash;
}

static uint64_t live_world_hash(void) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int x;
    int y;
    hash = hash_value(hash, (uint64_t)MAP_W);
    hash = hash_value(hash, (uint64_t)MAP_H);
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            const Tile *tile = &world[y][x];
            hash = hash_value(hash, (uint64_t)(unsigned int)tile->geography);
            hash = hash_value(hash, (uint64_t)(unsigned int)tile->climate);
            hash = hash_value(hash, (uint64_t)(unsigned int)tile->elevation);
            hash = hash_value(hash, (uint64_t)(unsigned int)tile->river);
        }
    }
    return hash;
}

static WorldGenConfig prior_config(void) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    config.seed = 519731u;
    config.random_seed = 0;
    config.ocean = 50;
    config.continent = 57;
    config.relief = 54;
    config.moisture = 52;
    config.drought = 48;
    config.vegetation = 55;
    config.bias_forest = 51;
    config.bias_desert = 49;
    config.bias_mountain = 53;
    config.bias_wetland = 47;
    return config;
}

static WorldGenConfig real_config(void) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    config.seed = 3300337u;
    config.random_seed = 0;
    config.ocean = 46;
    config.continent = 43;
    config.relief = 83;
    config.moisture = 85;
    config.drought = 16;
    config.vegetation = 70;
    config.bias_forest = 62;
    config.bias_desert = 72;
    config.bias_mountain = 46;
    config.bias_wetland = 64;
    return config;
}

static int commit_prior_world(int *revision, int *path_count,
                              RiverPath **paths, uint64_t *physical,
                              uint64_t *visible) {
    WorldGenConfig config = prior_config();
    WorldGenContext *context = world_gen_prepare_for_dimensions(
        &config, RIVER_E2E_PRIOR_W, RIVER_E2E_PRIOR_H);
    int ok;
    if (!context) return 0;
    map_w = RIVER_E2E_PRIOR_W;
    map_h = RIVER_E2E_PRIOR_H;
    ok = world_gen_prepared_can_commit(context, map_w, map_h) &&
         world_gen_commit_prepared(context);
    if (ok) {
        world_generated = 1;
        *revision = world_physical_state_revision();
        *path_count = river_path_count;
        *paths = river_paths;
        *physical = physical_hash();
        *visible = live_world_hash();
        ok = world_physical_state_valid() && *physical != 0 && *visible != 0 &&
             river_paths_validate(river_paths, river_path_count, map_w, map_h);
    }
    world_gen_release_prepared(context);
    return ok;
}

static int exact_staging_valid(const WorldGenContext *context) {
    const RiverGenerationDiagnostics *diag;
    if (!context || !context->staged_river_diagnostics_valid) return 0;
    diag = &context->staged_river_diagnostics;
    return context->width == MAX_MAP_W && context->height == MAX_MAP_H &&
        context->staged_river_paths_required == RIVER_E2E_REFERENCE_PATHS &&
        context->staged_river_path_count == RIVER_E2E_REFERENCE_PATHS &&
        context->staged_river_path_count > RIVER_E2E_FORMER_PATH_CAP &&
        context->staged_river_paths != NULL &&
        context->staged_river_token == context->hydrology_token &&
        diag->legacy_paths_required == context->staged_river_paths_required &&
        diag->legacy_paths_truncated == 0 &&
        diag->distributary_allocation_errors == 0 &&
        diag->segment_allocation_errors == 0 &&
        river_paths_validate((const RiverPath *)context->staged_river_paths,
                             context->staged_river_path_count,
                             context->width, context->height);
}

static int prewarm_contract(const RenderSnapshot *snapshot,
                            unsigned int *ready_mask, int *geometry_count,
                            int *lod3_visible, int *cache_stable,
                            RiverLodPolicyMetrics lod[4]) {
    const RiverRenderPath *paths;
    RenderStaticPhysicalOverlayCacheStats before = {0};
    RenderStaticPhysicalOverlayCacheStats after = {0};
    HDC hdc;
    int prewarmed;
    int selected = 1;
    int all_visible = 1;
    int index;
    render_static_physical_overlay_cache_invalidate();
    render_static_physical_overlay_cache_reset_debug_counters();
    hdc = GetDC(NULL);
    prewarmed = hdc &&
        render_static_physical_overlay_cache_prewarm_all(hdc, snapshot);
    if (prewarmed) before = *render_static_physical_overlay_cache_stats();
    for (index = 0; prewarmed && index < 4; index++)
        selected &= render_static_physical_overlay_cache_ensure_river(
            hdc, snapshot, index);
    if (prewarmed) after = *render_static_physical_overlay_cache_stats();
    if (hdc) ReleaseDC(NULL, hdc);
    *cache_stable = prewarmed && selected &&
        after.river_rebuilds == before.river_rebuilds &&
        after.river_surface_allocations == before.river_surface_allocations &&
        after.river_path_visits == before.river_path_visits &&
        after.persistent_bitmap_bytes == before.persistent_bitmap_bytes;
    *ready_mask = render_static_physical_overlay_cache_river_ready_mask(snapshot);
    paths = river_geometry_paths(geometry_count);
    *lod3_visible = 0;
    for (index = 0; index < 4; index++) lod[index] = river_lod_policy_metrics(index);
    for (index = 0; index < *geometry_count; index++) {
        int visible = river_render_path_visible_at_lod(&paths[index], 3);
        *lod3_visible += visible;
        all_visible &= paths[index].active && paths[index].point_count >= 2 && visible;
    }
    return prewarmed && *cache_stable && *ready_mask == 0x0fu &&
        *geometry_count == snapshot->rivers.path_count && all_visible &&
        lod[0].visible_paths * 100 >= snapshot->rivers.path_count * 15 &&
        lod[0].visible_paths * 100 <= snapshot->rivers.path_count * 20 &&
        lod[1].visible_paths * 100 >= snapshot->rivers.path_count * 35 &&
        lod[1].visible_paths * 100 <= snapshot->rivers.path_count * 45 &&
        lod[2].visible_paths * 100 >= snapshot->rivers.path_count * 70 &&
        lod[2].visible_paths * 100 <= snapshot->rivers.path_count * 80 &&
        lod[0].connected_paths == lod[0].visible_paths &&
        lod[1].connected_paths == lod[1].visible_paths &&
        lod[2].connected_paths == lod[2].visible_paths &&
        *lod3_visible == snapshot->rivers.path_count &&
        lod[3].target_paths == snapshot->rivers.path_count &&
        lod[3].visible_paths == snapshot->rivers.path_count;
}

static int corrupt_token_atomicity(WorldGenContext *context,
                                   const RenderSnapshot *snapshot,
                                   uint64_t expected_physical,
                                   uint64_t expected_world,
                                   uint64_t expected_river) {
    const RenderSnapshot *after;
    RiverPath *retry_paths = NULL;
    RiverPath *expected_paths = river_paths;
    size_t river_bytes = (size_t)river_path_count * sizeof(*river_paths);
    uint64_t snapshot_hash;
    int expected_revision = world_physical_state_revision();
    int clone_ready = 0;
    int rejected;
    int unchanged;
    if (context && !context->staged_river_paths &&
        context->hydrology_token != 0 && river_bytes > 0) {
        retry_paths = (RiverPath *)malloc(river_bytes);
    }
    if (retry_paths) {
        memcpy(retry_paths, river_paths, river_bytes);
        clone_ready = river_presentation_state_can_adopt(
                retry_paths, river_path_count, MAX_MAP_W, MAX_MAP_H) &&
            river_paths_validate(
                retry_paths, river_path_count, MAX_MAP_W, MAX_MAP_H) &&
            context->staged_river_diagnostics_valid &&
            context->staged_river_diagnostics.legacy_paths_required ==
                river_path_count;
        context->staged_river_paths = retry_paths;
        context->staged_river_path_count = river_path_count;
        context->staged_river_paths_required = river_path_count;
        context->staged_river_token = context->hydrology_token ^ UINT64_C(1);
    }
    snapshot_hash = hash_bytes(
        snapshot->rivers.paths,
        (size_t)snapshot->rivers.path_count * sizeof(*snapshot->rivers.paths));
    rejected = clone_ready && !world_gen_commit_prepared(context);
    after = render_snapshot_acquire();
    unchanged = clone_ready && rejected && after == snapshot &&
        after->revision == snapshot->revision &&
        after->rivers.paths == snapshot->rivers.paths &&
        after->rivers.path_count == snapshot->rivers.path_count &&
        hash_bytes(after->rivers.paths,
                   (size_t)after->rivers.path_count * sizeof(*after->rivers.paths)) ==
            snapshot_hash &&
        world_physical_state_revision() == expected_revision &&
        physical_hash() == expected_physical && live_world_hash() == expected_world &&
        river_paths == expected_paths && river_path_count == snapshot->rivers.path_count &&
        hash_bytes(river_paths, (size_t)river_path_count * sizeof(*river_paths)) ==
            expected_river;
    if (after) render_snapshot_release(after);
    free(context ? context->staged_river_paths : NULL);
    if (context) {
        context->staged_river_paths = NULL;
        context->staged_river_path_count = 0;
        context->staged_river_paths_required = 0;
        context->staged_river_token = 0;
    }
    return unchanged;
}

static int codec_roundtrip(int count, uint64_t expected_river,
                           long *river_file_bytes, long *physical_file_bytes,
                           uint64_t *loaded_river_hash,
                           uint64_t *loaded_physical_hash) {
    RiverPath *loaded = NULL;
    FILE *river_file = tmpfile();
    FILE *physical_file = tmpfile();
    size_t river_bytes = (size_t)count * sizeof(*river_paths);
    uint64_t before_physical = physical_hash();
    int river_ok = 0;
    int physical_ok = 0;
    if (river_file && map_save_river_paths_write(
            river_file, river_paths, count, MAX_MAP_W, MAX_MAP_H)) {
        *river_file_bytes = ftell(river_file);
        rewind(river_file);
        if (map_save_river_paths_allocate(&loaded, count, MAX_MAP_W, MAX_MAP_H) &&
            map_save_river_paths_read(
                river_file, loaded, count, MAX_MAP_W, MAX_MAP_H)) {
            *loaded_river_hash = hash_bytes(loaded, river_bytes);
            river_ok = *loaded_river_hash == expected_river &&
                memcmp(loaded, river_paths, river_bytes) == 0 &&
                *river_file_bytes == (long)river_bytes;
        }
    }
    if (physical_file && map_save_world_physical_write(
            physical_file, MAX_MAP_W, MAX_MAP_H)) {
        *physical_file_bytes = ftell(physical_file);
        rewind(physical_file);
        physical_ok = map_save_world_physical_read(
            physical_file, MAX_MAP_W, MAX_MAP_H);
        *loaded_physical_hash = physical_hash();
        physical_ok = physical_ok && before_physical != 0 &&
            *loaded_physical_hash == before_physical &&
            *physical_file_bytes ==
                (long)(32u + (size_t)MAX_MAP_W * MAX_MAP_H * 12u);
    }
    if (river_file) fclose(river_file);
    if (physical_file) fclose(physical_file);
    free(loaded);
    return river_ok && physical_ok;
}

int game_worldgen_river_e2e_probe_run(FILE *file) {
    WorldGenConfig config = real_config();
    WorldGenContext *context = NULL;
    const RenderSnapshot *snapshot = NULL;
    RiverLodPolicyMetrics lod[4] = {{0}};
    RiverGenerationDiagnostics committed_river_diagnostics = {0};
    WorldGenDiagnostics committed_world_diagnostics = {0};
    RiverPath *prior_paths = NULL;
    uint64_t prior_physical = 0;
    uint64_t prior_world = 0;
    uint64_t staged_hash = 0;
    uint64_t committed_physical = 0;
    uint64_t committed_world = 0;
    uint64_t committed_river = 0;
    uint64_t snapshot_river = 0;
    uint64_t loaded_river = 0;
    uint64_t loaded_physical = 0;
    long river_file_bytes = -1;
    long physical_file_bytes = -1;
    unsigned int ready_mask = 0;
    int prior_revision = 0;
    int prior_path_count = 0;
    int prior_ok;
    int prepare_atomic = 0;
    int exact = 0;
    int prevalidated = 0;
    int committed = 0;
    int replacement = 0;
    int committed_diagnostics_ok = 0;
    int snapshot_ok = 0;
    int prewarm_ok = 0;
    int cache_stable = 0;
    int geometry_count = 0;
    int lod3_visible = 0;
    int corrupt_ok = 0;
    int codec_ok = 0;
    int path_count = 0;
    int committed_revision = 0;
    int ok;
    if (!file) return 0;

    prior_ok = commit_prior_world(&prior_revision, &prior_path_count,
                                  &prior_paths, &prior_physical, &prior_world);
    if (prior_ok) context = world_gen_prepare_for_dimensions(
        &config, MAX_MAP_W, MAX_MAP_H);
    if (context) {
        prepare_atomic = world_physical_state_revision() == prior_revision &&
            river_paths == prior_paths && river_path_count == prior_path_count &&
            physical_hash() == prior_physical && live_world_hash() == prior_world;
        exact = exact_staging_valid(context);
        path_count = context->staged_river_path_count;
        if (exact) {
            staged_hash = hash_bytes(
                context->staged_river_paths,
                (size_t)path_count * sizeof(RiverPath));
            prevalidated = world_gen_prepared_can_commit(
                context, MAX_MAP_W, MAX_MAP_H);
        }
    }
    if (prevalidated) {
        map_w = MAX_MAP_W;
        map_h = MAX_MAP_H;
        committed = world_gen_commit_prepared(context);
        if (!committed) {
            map_w = RIVER_E2E_PRIOR_W;
            map_h = RIVER_E2E_PRIOR_H;
        }
    }
    if (committed) {
        world_generated = 1;
        committed_physical = physical_hash();
        committed_world = live_world_hash();
        committed_revision = world_physical_state_revision();
        committed_river = hash_bytes(
            river_paths, (size_t)river_path_count * sizeof(*river_paths));
        committed_diagnostics_ok =
            world_gen_last_committed_diagnostics(&committed_world_diagnostics) &&
            river_generation_committed_diagnostics(
                &committed_river_diagnostics) &&
            committed_world_diagnostics.river_segments_required == path_count &&
            committed_world_diagnostics.river_segments_copied == path_count &&
            committed_river_diagnostics.legacy_paths_required == path_count &&
            committed_river_diagnostics.legacy_paths_truncated == 0;
        replacement = world_physical_state_revision() != prior_revision &&
            river_paths != prior_paths && river_path_count == path_count &&
            river_path_count > RIVER_E2E_FORMER_PATH_CAP &&
            river_presentation_state_retained_bytes() ==
                (size_t)path_count * sizeof(*river_paths) &&
            committed_diagnostics_ok &&
            committed_physical != 0 && committed_physical != prior_physical &&
            committed_world != 0 && committed_world != prior_world &&
            committed_river == staged_hash;
        render_snapshot_init();
        dirty_mark_world();
        snapshot_ok = render_snapshot_publish_from_live_state_throttled(1);
        snapshot = render_snapshot_acquire();
        snapshot_ok = snapshot_ok && snapshot && snapshot->world_generated &&
            snapshot->map_w == MAX_MAP_W && snapshot->map_h == MAX_MAP_H &&
            snapshot->rivers.valid && snapshot->rivers.path_count == path_count &&
            snapshot->rivers.capacity == path_count;
        if (snapshot_ok) {
            snapshot_river = hash_bytes(
                snapshot->rivers.paths,
                (size_t)snapshot->rivers.path_count *
                    sizeof(*snapshot->rivers.paths));
            prewarm_ok = prewarm_contract(snapshot, &ready_mask,
                                          &geometry_count, &lod3_visible,
                                          &cache_stable, lod);
            corrupt_ok = corrupt_token_atomicity(
                context, snapshot, committed_physical,
                committed_world, committed_river);
        }
    }
    if (snapshot) render_snapshot_release(snapshot);
    if (committed && replacement && snapshot_ok && prewarm_ok && corrupt_ok) {
        codec_ok = codec_roundtrip(path_count, committed_river,
                                   &river_file_bytes, &physical_file_bytes,
                                   &loaded_river, &loaded_physical);
    }
    ok = prior_ok && context && prepare_atomic && exact && prevalidated &&
        committed && replacement && snapshot_ok && snapshot_river != 0 &&
        prewarm_ok && corrupt_ok && codec_ok;

    fprintf(file,
            "case=river_real_e2e_prepare seed=%u ocean=%d variant=1 "
            "paths=%d reference=%d former_cap=%d prior_paths=%d "
            "prior_revision=%d commit_revision=%d prepare_atomic=%d "
            "exact=%d prevalidated=%d committed=%d diagnostics=%d replacement=%d "
            "staged_hash=%016llx live_hash=%016llx ok=%d\n",
            config.seed, config.ocean, path_count, RIVER_E2E_REFERENCE_PATHS,
            RIVER_E2E_FORMER_PATH_CAP, prior_path_count, prior_revision,
            committed_revision, prepare_atomic, exact, prevalidated,
            committed, committed_diagnostics_ok, replacement,
            (unsigned long long)staged_hash,
            (unsigned long long)committed_river,
            prior_ok && context && prepare_atomic && exact && prevalidated &&
                committed && committed_diagnostics_ok && replacement);
    fprintf(file,
            "case=river_real_e2e_snapshot published=%d paths=%d geometry=%d "
            "ready_mask=0x%02x lod0=%d/%d lod1=%d/%d lod2=%d/%d "
            "lod3=%d/%d all_visible=%d cache_stable=%d snapshot_hash=%016llx "
            "corrupt_token_retained=%d ok=%d\n",
            snapshot_ok, path_count, geometry_count, ready_mask,
            lod[0].visible_paths, lod[0].target_paths,
            lod[1].visible_paths, lod[1].target_paths,
            lod[2].visible_paths, lod[2].target_paths,
            lod[3].visible_paths, lod[3].target_paths, lod3_visible, cache_stable,
            (unsigned long long)snapshot_river, corrupt_ok,
            snapshot_ok && prewarm_ok && corrupt_ok);
    fprintf(file,
            "case=river_real_e2e_save paths=%d river_bytes=%ld "
            "physical_bytes=%ld river_hash=%016llx/%016llx "
            "physical_hash=%016llx/%016llx codec=%d ok=%d\n",
            path_count, river_file_bytes, physical_file_bytes,
            (unsigned long long)committed_river,
            (unsigned long long)loaded_river,
            (unsigned long long)committed_physical,
            (unsigned long long)loaded_physical, codec_ok, codec_ok);
    fprintf(file, "case=river_real_e2e overall_ok=%d\n", ok);
    fflush(file);

    if (context) world_gen_release_prepared(context);
    render_static_physical_overlay_cache_invalidate();
    render_snapshot_shutdown();
    return ok;
}
