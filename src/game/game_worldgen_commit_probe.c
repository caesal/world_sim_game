#include "game/game_worldgen_commit_probe.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "core/render_snapshot.h"
#include "core/render_snapshot_keys.h"
#include "world/river_path_validation.h"
#include "world/river_presentation_state.h"
#include "world/world_gen.h"
#include "world/world_physical_state.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint64_t world_hash;
    uint64_t physical_hash;
    uint64_t path_hash;
    uint64_t snapshot_tile_hash;
    uint64_t snapshot_wind_hash;
    uint64_t snapshot_river_hash;
    uint64_t rng_hash;
    int map_w;
    int map_h;
    int physical_revision;
    int hydrology_revision;
    int physical_count;
    int path_count;
    unsigned int snapshot_revision;
    int snapshot_tile_revision;
    int snapshot_wind_revision;
    int snapshot_river_revision;
    int snapshot_sync_ok;
} CommitProbeCapture;

static uint64_t hash_start(void) {
    return UINT64_C(1469598103934665603);
}

static uint64_t hash_u64(uint64_t hash, uint64_t value) {
    int byte_index;
    for (byte_index = 0; byte_index < 8; byte_index++) {
        hash ^= (uint8_t)(value >> (byte_index * 8));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t committed_world_hash(int width, int height) {
    uint64_t hash = hash_start();
    int x;
    int y;
    hash = hash_u64(hash, (uint32_t)width);
    hash = hash_u64(hash, (uint32_t)height);
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            const Tile *tile = &world[y][x];
            hash = hash_u64(hash, (uint32_t)tile->geography);
            hash = hash_u64(hash, (uint32_t)tile->climate);
            hash = hash_u64(hash, (uint32_t)tile->ecology);
            hash = hash_u64(hash, (uint32_t)tile->resource);
            hash = hash_u64(hash, (uint32_t)tile->elevation);
            hash = hash_u64(hash, (uint32_t)tile->moisture);
            hash = hash_u64(hash, (uint32_t)tile->temperature);
            hash = hash_u64(hash, (uint32_t)tile->resource_variation);
            hash = hash_u64(hash, (uint32_t)tile->river);
        }
    }
    return hash;
}

static uint64_t committed_physical_hash(int *valid) {
    const WorldPhysicalTileState *tiles = world_physical_state_tiles();
    int count = world_physical_state_tile_count();
    uint64_t hash = hash_start();
    int i;
    *valid = world_physical_state_valid() && tiles != NULL && count > 0;
    hash = hash_u64(hash, (uint32_t)world_physical_state_width());
    hash = hash_u64(hash, (uint32_t)world_physical_state_height());
    hash = hash_u64(hash, (uint32_t)count);
    if (!*valid) return hash;
    for (i = 0; i < count; i++) {
        hash = hash_u64(hash, tiles[i].river_flow);
        hash = hash_u64(hash, tiles[i].river_width);
        hash = hash_u64(hash, tiles[i].wind_direction16);
        hash = hash_u64(hash, tiles[i].wind_speed);
        hash = hash_u64(hash, tiles[i].soil_fertility);
        hash = hash_u64(hash, tiles[i].river_order);
        hash = hash_u64(hash, tiles[i].river_flags);
    }
    return hash;
}

static uint64_t committed_path_hash(int *valid) {
    uint64_t hash = hash_start();
    int i;
    *valid = river_paths_validate(river_paths, river_path_count, MAP_W, MAP_H);
    hash = hash_u64(hash, (uint32_t)river_path_count);
    if (!*valid) return hash;
    for (i = 0; i < river_path_count; i++) {
        const RiverPath *path = &river_paths[i];
        int p;
        hash = hash_u64(hash, (uint32_t)path->active);
        hash = hash_u64(hash, (uint32_t)path->point_count);
        hash = hash_u64(hash, (uint32_t)path->width);
        hash = hash_u64(hash, (uint32_t)path->flow);
        hash = hash_u64(hash, (uint32_t)path->order);
        if (path->point_count < 0 || path->point_count > MAX_RIVER_POINTS) {
            *valid = 0;
            return hash;
        }
        for (p = 0; p < path->point_count; p++) {
            hash = hash_u64(hash, (uint32_t)path->points[p].x);
            hash = hash_u64(hash, (uint32_t)path->points[p].y);
        }
    }
    return hash;
}

static uint64_t snapshot_tile_hash(const RenderSnapshot *snapshot) {
    uint64_t hash = hash_start();
    int count = snapshot->map_w * snapshot->map_h;
    int i;
    hash = hash_u64(hash, (uint32_t)snapshot->map_w);
    hash = hash_u64(hash, (uint32_t)snapshot->map_h);
    for (i = 0; i < count; i++) {
        const SnapshotTile *tile = &snapshot->tiles[i];
        hash = hash_u64(hash, tile->geography);
        hash = hash_u64(hash, tile->climate);
        hash = hash_u64(hash, tile->ecology);
        hash = hash_u64(hash, tile->resource);
        hash = hash_u64(hash, tile->river);
        hash = hash_u64(hash, tile->elevation);
        hash = hash_u64(hash, tile->water_depth);
        hash = hash_u64(hash, tile->water_deep_percent);
        hash = hash_u64(hash, (uint16_t)tile->moisture);
        hash = hash_u64(hash, (uint16_t)tile->temperature);
    }
    return hash;
}

static uint64_t hash_wind_samples(uint64_t hash, const SnapshotWindSample *samples,
                                  int count, int capacity, int *valid) {
    int i;
    if (count < 0 || count > capacity) {
        *valid = 0;
        return hash;
    }
    hash = hash_u64(hash, (uint32_t)count);
    for (i = 0; i < count; i++) {
        hash = hash_u64(hash, samples[i].x);
        hash = hash_u64(hash, samples[i].y);
        hash = hash_u64(hash, samples[i].direction);
        hash = hash_u64(hash, samples[i].speed);
    }
    return hash;
}

static uint64_t snapshot_wind_hash(const SnapshotWindField *wind, int *valid) {
    uint64_t hash = hash_start();
    *valid = wind->valid != 0;
    hash = hash_u64(hash, (uint32_t)wind->map_w);
    hash = hash_u64(hash, (uint32_t)wind->map_h);
    hash = hash_wind_samples(hash, wind->coarse, wind->coarse_count,
                             SNAPSHOT_WIND_COARSE_MAX, valid);
    hash = hash_wind_samples(hash, wind->medium, wind->medium_count,
                             SNAPSHOT_WIND_MEDIUM_MAX, valid);
    return hash_wind_samples(hash, wind->fine, wind->fine_count,
                             SNAPSHOT_WIND_FINE_MAX, valid);
}

static uint64_t snapshot_river_hash(const SnapshotRiverField *rivers, int *valid) {
    uint64_t hash = hash_start();
    int i;
    *valid = rivers->valid && rivers->capacity == rivers->path_count &&
             river_path_count_valid(rivers->path_count,
                                    rivers->map_w, rivers->map_h) &&
             (rivers->path_count == 0 ? !rivers->paths : rivers->paths != NULL);
    hash = hash_u64(hash, (uint32_t)rivers->map_w);
    hash = hash_u64(hash, (uint32_t)rivers->map_h);
    hash = hash_u64(hash, (uint32_t)rivers->path_count);
    if (!*valid) return hash;
    for (i = 0; i < rivers->path_count; i++) {
        const SnapshotRiverPath *path = &rivers->paths[i];
        int p;
        hash = hash_u64(hash, path->flow);
        hash = hash_u64(hash, path->point_count);
        hash = hash_u64(hash, path->width);
        hash = hash_u64(hash, path->order);
        hash = hash_u64(hash, path->semantic_flags);
        hash = hash_u64(hash, path->end_flags);
        if (path->point_count > MAX_RIVER_POINTS) {
            *valid = 0;
            return hash;
        }
        for (p = 0; p < path->point_count; p++) {
            hash = hash_u64(hash, path->points[p].x);
            hash = hash_u64(hash, path->points[p].y);
            hash = hash_u64(hash, path->points[p].semantic_flags);
        }
    }
    return hash;
}

static int capture_published_snapshot(CommitProbeCapture *capture) {
    const RenderSnapshot *snapshot = render_snapshot_acquire();
    int wind_valid;
    int river_valid;
    if (!snapshot) return 0;
    capture->snapshot_revision = snapshot->revision;
    capture->snapshot_tile_revision = snapshot->tiles_revision;
    capture->snapshot_wind_revision = snapshot->wind_revision;
    capture->snapshot_river_revision = snapshot->river_revision;
    capture->snapshot_tile_hash = snapshot_tile_hash(snapshot);
    capture->snapshot_wind_hash = snapshot_wind_hash(&snapshot->wind, &wind_valid);
    capture->snapshot_river_hash = snapshot_river_hash(&snapshot->rivers, &river_valid);
    capture->snapshot_sync_ok = wind_valid && river_valid &&
        snapshot->map_w == capture->map_w && snapshot->map_h == capture->map_h &&
        snapshot->wind.map_w == capture->map_w && snapshot->wind.map_h == capture->map_h &&
        snapshot->rivers.map_w == capture->map_w && snapshot->rivers.map_h == capture->map_h &&
        snapshot->tiles_revision == render_snapshot_tile_revision_key() &&
        snapshot->wind_revision == render_snapshot_wind_revision_key() &&
        snapshot->river_revision == render_snapshot_river_revision_key() &&
        snapshot->wind.revision == snapshot->wind_revision &&
        snapshot->rivers.revision == snapshot->river_revision &&
        snapshot->rivers.path_count == river_path_count;
    render_snapshot_release(snapshot);
    return capture->snapshot_sync_ok;
}

static uint64_t downstream_rng_signature(void) {
    uint64_t hash = hash_start();
    int i;
    for (i = 0; i < 24; i++) hash = hash_u64(hash, (uint32_t)rand());
    return hash;
}

static int capture_live(CommitProbeCapture *capture, int publish) {
    int physical_valid;
    int path_valid;
    memset(capture, 0, sizeof(*capture));
    capture->map_w = MAP_W;
    capture->map_h = MAP_H;
    capture->physical_revision = world_physical_state_revision();
    capture->hydrology_revision = dirty_revision_hydrology();
    capture->physical_count = world_physical_state_tile_count();
    capture->path_count = river_path_count;
    capture->world_hash = committed_world_hash(MAP_W, MAP_H);
    capture->physical_hash = committed_physical_hash(&physical_valid);
    capture->path_hash = committed_path_hash(&path_valid);
    if (publish) render_snapshot_publish_from_live_state();
    return physical_valid && path_valid && capture_published_snapshot(capture);
}

static WorldGenConfig config_a(void) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    config.seed = UINT32_C(1242689025);
    config.random_seed = 0;
    config.ocean = 53;
    config.continent = 57;
    config.relief = 64;
    config.moisture = 58;
    config.drought = 42;
    config.vegetation = 61;
    config.bias_forest = 56;
    config.bias_desert = 44;
    config.bias_mountain = 67;
    config.bias_wetland = 52;
    return config;
}

static WorldGenConfig config_b(void) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    config.seed = UINT32_C(2967407039);
    config.random_seed = 0;
    config.ocean = 46;
    config.continent = 63;
    config.relief = 38;
    config.moisture = 72;
    config.drought = 31;
    config.vegetation = 49;
    config.bias_forest = 43;
    config.bias_desert = 58;
    config.bias_mountain = 36;
    config.bias_wetland = 69;
    return config;
}

static int path_validation_contract(FILE *file, int width, int height) {
    RiverPath path;
    RiverPath *owned = NULL;
    int point;
    int valid_max;
    int valid_above_former_cap;
    int rejects_limit_plus_one;
    int zero_valid;
    int zero_adopt;
    int rejects_short;
    int rejects_bounds;
    int rejects_repeat;
    int rejects_inactive;
    memset(&path, 0, sizeof(path));
    path.active = 1;
    path.point_count = MAX_RIVER_POINTS;
    path.width = 1;
    path.flow = 1;
    path.order = 1;
    for (point = 0; point < path.point_count; point++) {
        path.points[point].x = point;
        path.points[point].y = 0;
    }
    valid_max = river_paths_validate(&path, 1, width, height);
    path.point_count = 1;
    rejects_short = !river_paths_validate(&path, 1, width, height);
    path.point_count = MAX_RIVER_POINTS;
    path.points[2].x = width;
    rejects_bounds = !river_paths_validate(&path, 1, width, height);
    path.points[2].x = path.points[1].x;
    rejects_repeat = !river_paths_validate(&path, 1, width, height);
    path.points[2].x = 2;
    path.active = 0;
    rejects_inactive = !river_paths_validate(&path, 1, width, height);
    valid_above_former_cap = river_path_count_valid(12289, width, height);
    rejects_limit_plus_one = !river_path_count_valid(
        river_path_count_limit(width, height) + 1, width, height);
    zero_valid = river_paths_validate(NULL, 0, width, height);
    zero_adopt = river_presentation_state_adopt(&owned, 0, width, height) &&
                 !owned && !river_paths && river_path_count == 0;
    fprintf(file, "case=river_path_validation max=%d short=%d bounds=%d repeat=%d inactive=%d "
                  "above_12288=%d limit_plus_one=%d zero_valid=%d zero_adopt=%d ok=%d\n",
            valid_max, rejects_short, rejects_bounds, rejects_repeat, rejects_inactive,
            valid_above_former_cap, rejects_limit_plus_one, zero_valid, zero_adopt,
            valid_max && rejects_short && rejects_bounds && rejects_repeat && rejects_inactive &&
            valid_above_former_cap && rejects_limit_plus_one && zero_valid && zero_adopt);
    return valid_max && rejects_short && rejects_bounds && rejects_repeat && rejects_inactive &&
           valid_above_former_cap && rejects_limit_plus_one && zero_valid && zero_adopt;
}

static int prepare_commit_capture(FILE *file, const char *label,
                                  const WorldGenConfig *config,
                                  int width, int height,
                                  CommitProbeCapture *capture) {
    WorldGenContext *context = world_gen_prepare_for_dimensions(config, width, height);
    int prevalidated = context && world_gen_prepared_can_commit(context, width, height);
    int committed = prevalidated && world_gen_commit_prepared(context);
    int captured = 0;
    uint64_t rng_hash = 0;
    memset(capture, 0, sizeof(*capture));
    if (committed) {
        world_generated = 0;
        rng_hash = downstream_rng_signature();
        dirty_mark_world();
        captured = capture_live(capture, 1);
        capture->rng_hash = rng_hash;
    }
    fprintf(file,
            "case=commit_sequence label=%s seed=%u prepared=%d prevalidated=%d "
            "committed=%d captured=%d world=%016llx physical=%016llx paths=%016llx "
            "snapshot_tiles=%016llx snapshot_wind=%016llx snapshot_rivers=%016llx "
            "rng=%016llx map=%dx%d physical_count=%d paths=%d physical_rev=%d "
            "hydrology_rev=%d snapshot_rev=%u snapshot_keys=%d/%d/%d sync=%d\n",
            label, config->seed, context != NULL, prevalidated, committed, captured,
            (unsigned long long)capture->world_hash,
            (unsigned long long)capture->physical_hash,
            (unsigned long long)capture->path_hash,
            (unsigned long long)capture->snapshot_tile_hash,
            (unsigned long long)capture->snapshot_wind_hash,
            (unsigned long long)capture->snapshot_river_hash,
            (unsigned long long)capture->rng_hash,
            capture->map_w, capture->map_h, capture->physical_count, capture->path_count,
            capture->physical_revision, capture->hydrology_revision,
            capture->snapshot_revision, capture->snapshot_tile_revision,
            capture->snapshot_wind_revision, capture->snapshot_river_revision,
            capture->snapshot_sync_ok);
    world_gen_release_prepared(context);
    return committed && captured;
}

static int same_payload(const CommitProbeCapture *a, const CommitProbeCapture *b) {
    return a->world_hash == b->world_hash &&
           a->physical_hash == b->physical_hash &&
           a->path_hash == b->path_hash &&
           a->snapshot_tile_hash == b->snapshot_tile_hash &&
           a->snapshot_wind_hash == b->snapshot_wind_hash &&
           a->snapshot_river_hash == b->snapshot_river_hash &&
           a->rng_hash == b->rng_hash;
}

static int distinct_payload(const CommitProbeCapture *a, const CommitProbeCapture *b) {
    return a->world_hash != b->world_hash &&
           a->physical_hash != b->physical_hash &&
           a->path_hash != b->path_hash &&
           a->snapshot_tile_hash != b->snapshot_tile_hash &&
           a->snapshot_wind_hash != b->snapshot_wind_hash &&
           a->snapshot_river_hash != b->snapshot_river_hash &&
           a->rng_hash != b->rng_hash;
}

static int mismatched_commit_is_atomic(FILE *file, const WorldGenConfig *config,
                                       int small_w, int small_h) {
    WorldGenContext *context = world_gen_prepare_for_dimensions(config, small_w, small_h);
    CommitProbeCapture before;
    CommitProbeCapture after;
    int medium_w;
    int medium_h;
    int prepared = context != NULL;
    int valid_small = prepared && world_gen_prepared_can_commit(context, small_w, small_h);
    int invalid_medium;
    int committed;
    int unchanged;
    map_size_dimensions(MAP_SIZE_MEDIUM, &medium_w, &medium_h);
    capture_live(&before, 0);
    invalid_medium = prepared && !world_gen_prepared_can_commit(context, medium_w, medium_h);
    set_active_map_size(MAP_SIZE_MEDIUM);
    committed = prepared && world_gen_commit_prepared(context);
    set_active_map_size(MAP_SIZE_SMALL);
    capture_live(&after, 0);
    unchanged = before.world_hash == after.world_hash &&
                before.physical_hash == after.physical_hash &&
                before.path_hash == after.path_hash &&
                before.snapshot_tile_hash == after.snapshot_tile_hash &&
                before.snapshot_wind_hash == after.snapshot_wind_hash &&
                before.snapshot_river_hash == after.snapshot_river_hash &&
                before.physical_revision == after.physical_revision &&
                before.hydrology_revision == after.hydrology_revision &&
                before.snapshot_revision == after.snapshot_revision &&
                before.snapshot_tile_revision == after.snapshot_tile_revision &&
                before.snapshot_wind_revision == after.snapshot_wind_revision &&
                before.snapshot_river_revision == after.snapshot_river_revision &&
                after.map_w == small_w && after.map_h == small_h;
    fprintf(file,
            "case=commit_mismatch_atomic prepared=%d valid_small=%d invalid_medium=%d "
            "commit_rejected=%d hashes_revisions_unchanged=%d snapshot_rev=%u/%u "
            "physical_rev=%d/%d hydrology_rev=%d/%d ok=%d\n",
            prepared, valid_small, invalid_medium, !committed, unchanged,
            before.snapshot_revision, after.snapshot_revision,
            before.physical_revision, after.physical_revision,
            before.hydrology_revision, after.hydrology_revision,
            prepared && valid_small && invalid_medium && !committed && unchanged);
    world_gen_release_prepared(context);
    return prepared && valid_small && invalid_medium && !committed && unchanged;
}

int game_worldgen_commit_probe_run(FILE *file) {
    WorldGenConfig a = config_a();
    WorldGenConfig b = config_b();
    CommitProbeCapture a_fresh;
    CommitProbeCapture b_middle;
    CommitProbeCapture a_repeat;
    int width;
    int height;
    int sequence_ok;
    int repeat_ok;
    int distinct_ok;
    int monotonic_ok;
    int mismatch_ok;
    int ok;
    if (!file) return 0;
    map_size_dimensions(MAP_SIZE_SMALL, &width, &height);
    set_active_map_size(MAP_SIZE_SMALL);
    world_generated = 0;
    render_snapshot_init();
    sequence_ok = path_validation_contract(file, width, height);
    sequence_ok &= prepare_commit_capture(file, "a_fresh", &a, width, height, &a_fresh);
    sequence_ok &= prepare_commit_capture(file, "b_middle", &b, width, height, &b_middle);
    sequence_ok &= prepare_commit_capture(file, "a_repeat", &a, width, height, &a_repeat);
    repeat_ok = sequence_ok && same_payload(&a_fresh, &a_repeat);
    distinct_ok = sequence_ok && distinct_payload(&a_fresh, &b_middle);
    monotonic_ok = sequence_ok &&
        a_fresh.physical_revision < b_middle.physical_revision &&
        b_middle.physical_revision < a_repeat.physical_revision &&
        a_fresh.hydrology_revision < b_middle.hydrology_revision &&
        b_middle.hydrology_revision < a_repeat.hydrology_revision &&
        a_fresh.snapshot_revision < b_middle.snapshot_revision &&
        b_middle.snapshot_revision < a_repeat.snapshot_revision;
    mismatch_ok = sequence_ok && mismatched_commit_is_atomic(file, &a, width, height);
    ok = sequence_ok && repeat_ok && distinct_ok && monotonic_ok && mismatch_ok &&
         a_fresh.snapshot_sync_ok && b_middle.snapshot_sync_ok && a_repeat.snapshot_sync_ok;
    fprintf(file,
            "case=commit_aba repeat_equal=%d b_distinct_all_hashes=%d "
            "revisions_monotonic=%d snapshots_synchronized=%d mismatch_atomic=%d ok=%d\n",
            repeat_ok, distinct_ok, monotonic_ok,
            a_fresh.snapshot_sync_ok && b_middle.snapshot_sync_ok && a_repeat.snapshot_sync_ok,
            mismatch_ok, ok);
    return ok;
}
