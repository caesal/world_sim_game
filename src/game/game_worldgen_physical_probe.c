#include "game_worldgen_physical_probe.h"

#include "core/game_types.h"
#include "core/world_types.h"
#include "game/game_worldgen_hydrology_probe.h"
#include "game/game_worldgen_climate_probe.h"
#include "io/map_save_world_physical.h"
#include "world/mountain_gen.h"
#include "world/river_path_validation.h"
#include "world/wind_vector.h"
#include "world/world_gen_aridity_response.h"
#include "world/world_gen_classify.h"
#include "world/world_gen_climate.h"
#include "world/world_gen_context.h"
#include "world/world_gen_elevation.h"
#include "world/world_physical_state.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int circular_direction_delta(int first, int second) {
    int delta = abs(first - second);
    return delta < 16 - delta ? delta : 16 - delta;
}

static int build_climate_only(WorldGenContext *context, const WorldGenConfig *config,
                              int width, int height) {
    if (!world_gen_context_create(context, config, width, height, config->seed)) return 0;
    if (!world_gen_build_elevation_and_mask(context) ||
        !world_gen_apply_mountains(context)) return 0;
    world_gen_finalize_elevation(context);
    return world_gen_build_climate_fields(context);
}

static int check_climate_repeat(FILE *file, const WorldGenConfig *base_config) {
    WorldGenContext first;
    WorldGenContext second;
    WorldGenConfig config = *base_config;
    int width;
    int height;
    int first_ok;
    int second_ok;
    int ok;
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    config.random_seed = 0;
    map_size_dimensions(MAP_SIZE_SMALL, &width, &height);
    first_ok = build_climate_only(&first, &config, width, height);
    ok = first_ok && game_worldgen_climate_probe_check_context(
        file, "climate_repeat_first", &first, 0);
    second_ok = build_climate_only(&second, &config, width, height);
    ok &= second_ok && game_worldgen_climate_probe_check_context(
        file, "climate_repeat_second", &second, 0);
    ok &= first_ok && second_ok && game_worldgen_climate_probe_compare(
        file, "climate_repeat", &first, &second);
    world_gen_context_destroy(&first);
    world_gen_context_destroy(&second);
    return ok;
}

static int check_land_and_labels(FILE *file, const char *label,
                                 const WorldGenContext *context) {
    int expected_land = world_gen_land_mask_target_tiles(
        context->config.ocean, context->tile_count);
    int land = 0;
    int elevation_errors = 0;
    int label_errors = 0;
    int value_errors = 0;
    int semantic_errors = 0;
    int i;
    for (i = 0; i < context->tile_count; i++) {
        int is_land = context->land_mask[i] != 0;
        Geography geography = (Geography)context->geography[i];
        Climate climate = (Climate)context->climate[i];
        Ecology ecology = (Ecology)context->ecology[i];
        ResourceFeature resource = (ResourceFeature)context->resource[i];
        uint16_t river_flags = context->river_flags[i];
        land += is_land;
        if ((is_land && context->elevation[i] <= context->sea_level) ||
            (!is_land && context->elevation[i] >= context->sea_level)) elevation_errors++;
        if (context->geography[i] >= GEO_COUNT || context->climate[i] >= CLIMATE_COUNT ||
            context->ecology[i] >= ECO_COUNT || context->resource[i] >= RESOURCE_FEATURE_COUNT) {
            label_errors++;
        }
        if (context->moisture[i] < 0 || context->moisture[i] > 100 ||
            context->temperature[i] < 0 || context->temperature[i] > 100 ||
            context->precipitation[i] < 0 || context->precipitation[i] > 100 ||
            context->soil_fertility[i] > 100) value_errors++;
        if (context->geography[i] == GEO_PLAIN && !is_land) label_errors++;
        if (!is_land && context->ecology[i] != ECO_NONE) label_errors++;
        if (!is_land && (geography != GEO_OCEAN && geography != GEO_BAY)) semantic_errors++;
        if (!is_land && climate != CLIMATE_OCEANIC) semantic_errors++;
        if ((geography == GEO_OCEAN && resource != RESOURCE_FEATURE_NONE) ||
            (geography == GEO_BAY && resource != RESOURCE_FEATURE_FISHERY)) semantic_errors++;
        if (geography == GEO_LAKE && (!(river_flags & WORLD_GEN_RIVER_LAKE) ||
            ecology != ECO_NONE || (resource != RESOURCE_FEATURE_FISHERY &&
                                     resource != RESOURCE_FEATURE_SALT_LAKE))) semantic_errors++;
        if ((river_flags & WORLD_GEN_RIVER_SALT_LAKE) &&
            (!(river_flags & WORLD_GEN_RIVER_LAKE) ||
             !(river_flags & WORLD_GEN_RIVER_CLOSED_BASIN) ||
             resource != RESOURCE_FEATURE_SALT_LAKE)) semantic_errors++;
        if (resource == RESOURCE_FEATURE_SALT_LAKE &&
            !(river_flags & WORLD_GEN_RIVER_SALT_LAKE)) semantic_errors++;
        if ((geography == GEO_DELTA && !(river_flags & WORLD_GEN_RIVER_DELTA)) ||
            (geography == GEO_OASIS &&
             !world_gen_classify_response_visible_oasis_for_pair(
                 context, i, climate, world_gen_aridity_response_oasis_drop(),
                 world_gen_aridity_response_transition_margin()))) semantic_errors++;
        if ((ecology == ECO_MANGROVE && geography != GEO_DELTA) ||
            (ecology == ECO_SWAMP && geography != GEO_WETLAND && geography != GEO_BASIN) ||
            (ecology == ECO_DESERT && climate != CLIMATE_DESERT)) semantic_errors++;
    }
    fprintf(file, "case=physical_land label=%s map=%dx%d land=%d expected=%d "
                  "elevation_errors=%d label_errors=%d value_errors=%d semantic_errors=%d ok=%d\n",
            label, context->width, context->height, land, expected_land,
            elevation_errors, label_errors, value_errors, semantic_errors,
            land == expected_land && elevation_errors == 0 && label_errors == 0 &&
            value_errors == 0 && semantic_errors == 0);
    return land == expected_land && elevation_errors == 0 && label_errors == 0 &&
           value_errors == 0 && semantic_errors == 0;
}

static int check_mountains(FILE *file, const char *label,
                           const WorldGenContext *context, int require_contrast) {
    int core = 0;
    int shoulder = 0;
    int foothill = 0;
    int isolated = 0;
    int core_shoulder_edges = 0;
    int shoulder_foothill_edges = 0;
    int i;
    for (i = 0; i < context->tile_count; i++) {
        int uplift = context->mountain_uplift[i];
        if (uplift >= 18) {
            int x = i % context->width;
            int y = i / context->width;
            int neighbors = 0;
            int dx;
            int dy;
            core++;
            for (dy = -1; dy <= 1; dy++) {
                for (dx = -1; dx <= 1; dx++) {
                    int next;
                    if ((dx == 0 && dy == 0) ||
                        !world_gen_context_in_bounds(context, x + dx, y + dy)) continue;
                    next = world_gen_context_index(context, x + dx, y + dy);
                    if (context->mountain_uplift[next] >= 7) neighbors++;
                }
            }
            if (neighbors < 3) isolated++;
        } else if (uplift >= 7) shoulder++;
        else if (uplift > 0) foothill++;
        if (i % context->width + 1 < context->width) {
            int next = context->mountain_uplift[i + 1];
            if ((uplift >= 18 && next >= 7 && next < 18) ||
                (next >= 18 && uplift >= 7 && uplift < 18)) core_shoulder_edges++;
            if ((uplift >= 7 && uplift < 18 && next > 0 && next < 7) ||
                (next >= 7 && next < 18 && uplift > 0 && uplift < 7)) shoulder_foothill_edges++;
        }
    }
    {
        int shape_ok = core > 0 && shoulder > 0 && foothill > 0 &&
                       core_shoulder_edges > 0 && shoulder_foothill_edges > 0 &&
                       isolated * 10 <= core + 9;
        int contrast_ok = !require_contrast || (shoulder >= core / 5 && foothill >= core / 5);
        fprintf(file, "case=physical_mountains label=%s core=%d shoulder=%d foothill=%d "
                      "core_shoulder_edges=%d shoulder_foothill_edges=%d isolated=%d strict=%d ok=%d\n",
                label, core, shoulder, foothill, core_shoulder_edges,
                shoulder_foothill_edges, isolated, require_contrast,
                shape_ok && contrast_ok);
        return shape_ok && contrast_ok;
    }
}

static int check_wind(FILE *file, const char *label, const WorldGenContext *context) {
    int bounds_errors = 0;
    int discontinuities = 0;
    int neighbor_pairs = 0;
    int y;
    int x;
    for (y = 0; y < context->height; y++) {
        for (x = 0; x < context->width; x++) {
            int index = world_gen_context_index(context, x, y);
            if (context->wind_direction16[index] >= 16 || context->wind_speed[index] > 100) {
                bounds_errors++;
            }
            if (x + 1 < context->width) {
                int next = index + 1;
                neighbor_pairs++;
                if (circular_direction_delta(context->wind_direction16[index],
                                             context->wind_direction16[next]) > 4) discontinuities++;
            }
            if (y + 1 < context->height) {
                int next = index + context->width;
                neighbor_pairs++;
                if (circular_direction_delta(context->wind_direction16[index],
                                             context->wind_direction16[next]) > 4) discontinuities++;
            }
        }
    }
    fprintf(file, "case=physical_wind label=%s bounds_errors=%d discontinuities=%d pairs=%d ok=%d\n",
            label, bounds_errors, discontinuities, neighbor_pairs,
            bounds_errors == 0 && discontinuities * 10 <= neighbor_pairs);
    return bounds_errors == 0 && discontinuities * 10 <= neighbor_pairs;
}

static void wind_offset_tile(int direction, int x, int y, int distance,
                             int *out_x, int *out_y) {
    int x_q10;
    int y_q10;
    wind_vector_offset_point_q10(direction, x, y, distance * WIND_VECTOR_SCALE, 0,
                                 &x_q10, &y_q10);
    *out_x = x_q10 >= 0
        ? (x_q10 + WIND_VECTOR_SCALE / 2) / WIND_VECTOR_SCALE
        : -(-x_q10 + WIND_VECTOR_SCALE / 2) / WIND_VECTOR_SCALE;
    *out_y = y_q10 >= 0
        ? (y_q10 + WIND_VECTOR_SCALE / 2) / WIND_VECTOR_SCALE
        : -(-y_q10 + WIND_VECTOR_SCALE / 2) / WIND_VECTOR_SCALE;
}

static int check_rain_shadow(FILE *file, const char *label,
                             const WorldGenContext *context, int required) {
    uint64_t windward_total = 0;
    uint64_t leeward_total = 0;
    int pairs = 0;
    int i;
    for (i = 0; i < context->tile_count; i++) {
        int x;
        int y;
        int lee_x;
        int lee_y;
        int lee;
        if (context->mountain_uplift[i] < 18) continue;
        x = i % context->width;
        y = i / context->width;
        wind_offset_tile(context->wind_direction16[i], x, y, 4, &lee_x, &lee_y);
        if (!world_gen_context_in_bounds(context, lee_x, lee_y)) continue;
        lee = world_gen_context_index(context, lee_x, lee_y);
        if (!context->land_mask[lee] || context->elevation[i] < context->elevation[lee] + 5) continue;
        windward_total += (uint16_t)context->precipitation[i];
        leeward_total += (uint16_t)context->precipitation[lee];
        pairs++;
    }
    {
        int contrast = pairs > 0 && windward_total > leeward_total;
        int ok = !required || (pairs >= 4 && contrast);
        fprintf(file, "case=physical_rain_shadow label=%s pairs=%d windward=%llu leeward=%llu "
                      "required=%d ok=%d\n",
                label, pairs, (unsigned long long)windward_total,
                (unsigned long long)leeward_total, required, ok);
        return ok;
    }
}

static int commit_context_physical_fields(const WorldGenContext *context) {
    WorldPhysicalStateInput input;
    memset(&input, 0, sizeof(input));
    input.map_w = context->width;
    input.map_h = context->height;
    input.tile_count = context->tile_count;
    input.river_flow = context->river_flow;
    input.river_width = context->river_width;
    input.wind_direction16 = context->wind_direction16;
    input.wind_speed = context->wind_speed;
    input.soil_fertility = context->soil_fertility;
    input.river_order = context->river_order;
    input.river_flags = context->river_flags;
    return world_physical_state_commit_fields(&input);
}

static int physical_state_matches(const WorldGenContext *context) {
    const WorldPhysicalTileState *tiles = world_physical_state_tiles();
    int i;
    if (!tiles || world_physical_state_tile_count() != context->tile_count) return 0;
    for (i = 0; i < context->tile_count; i++) {
        if (tiles[i].river_flow != context->river_flow[i] ||
            tiles[i].river_width != context->river_width[i] ||
            tiles[i].wind_direction16 != context->wind_direction16[i] ||
            tiles[i].wind_speed != context->wind_speed[i] ||
            tiles[i].soil_fertility != context->soil_fertility[i] ||
            tiles[i].river_order != context->river_order[i] ||
            tiles[i].river_flags != context->river_flags[i]) return 0;
    }
    return 1;
}

static int check_phy20(FILE *file, const char *label, const WorldGenContext *context) {
    FILE *roundtrip = tmpfile();
    FILE *corrupt = tmpfile();
    FILE *legacy = tmpfile();
    uint32_t invalid_version = MAP_SAVE_WORLD_PHYSICAL_BLOCK_VERSION + 1u;
    uint32_t legacy_version = 1u;
    int write_ok = 0;
    int roundtrip_ok = 0;
    int corruption_ok = 0;
    int legacy_rejected = 0;
    if (roundtrip && commit_context_physical_fields(context)) {
        write_ok = map_save_world_physical_write(roundtrip, context->width, context->height);
        rewind(roundtrip);
        world_physical_state_reset();
        roundtrip_ok = write_ok && map_save_world_physical_read(roundtrip, context->width, context->height) &&
                       physical_state_matches(context);
    }
    if (corrupt && commit_context_physical_fields(context) &&
        map_save_world_physical_write(corrupt, context->width, context->height)) {
        fflush(corrupt);
        fseek(corrupt, 8L, SEEK_SET);
        fwrite(&invalid_version, sizeof(invalid_version), 1, corrupt);
        rewind(corrupt);
        world_physical_state_reset();
        corruption_ok = !map_save_world_physical_read(corrupt, context->width, context->height);
    }
    if (legacy && commit_context_physical_fields(context) &&
        map_save_world_physical_write(legacy, context->width, context->height)) {
        fflush(legacy);
        fseek(legacy, 8L, SEEK_SET);
        fwrite(&legacy_version, sizeof(legacy_version), 1, legacy);
        rewind(legacy);
        world_physical_state_reset();
        legacy_rejected = !map_save_world_physical_read(legacy, context->width, context->height);
    }
    if (roundtrip) fclose(roundtrip);
    if (corrupt) fclose(corrupt);
    if (legacy) fclose(legacy);
    fprintf(file, "case=physical_phy20 label=%s block=%d write=%d roundtrip=%d "
                  "corruption_rejected=%d v1_rejected=%d ok=%d\n",
            label, MAP_SAVE_WORLD_PHYSICAL_BLOCK_VERSION, write_ok, roundtrip_ok,
            corruption_ok, legacy_rejected,
            write_ok && roundtrip_ok && corruption_ok && legacy_rejected);
    return write_ok && roundtrip_ok && corruption_ok && legacy_rejected;
}

int game_worldgen_physical_probe_check_context(FILE *file, const char *label,
                                               const WorldGenContext *context,
                                               int require_mountain_contrast,
                                               int check_save) {
    const WorldGenDiagnostics *diagnostics = world_gen_last_diagnostics();
    int ok;
    if (!file || !label || !context || !diagnostics) return 0;
    ok = check_land_and_labels(file, label, context);
    ok &= game_worldgen_climate_probe_check_context(file, label, context,
                                                     require_mountain_contrast);
    ok &= check_mountains(file, label, context, require_mountain_contrast);
    ok &= check_wind(file, label, context);
    ok &= check_rain_shadow(file, label, context, require_mountain_contrast);
    if (check_save) ok &= check_phy20(file, label, context);
    {
        int timing_ok = diagnostics->context_bytes > 0 &&
                        diagnostics->context_bytes <= (size_t)128 * 1024u * 1024u &&
                        diagnostics->hydrology_bytes > 0 &&
                        diagnostics->peak_bytes >= diagnostics->context_bytes +
                                                   diagnostics->hydrology_bytes &&
                        diagnostics->physical_hash != 0;
        fprintf(file, "case=physical_timing label=%s elevation_ms=%llu mountain_ms=%llu "
                      "climate_ms=%llu hydrology_ms=%llu classification_ms=%llu total_ms=%llu "
                      "context_bytes=%llu hydro_bytes=%llu path_bytes=%llu peak_bytes=%llu "
                      "hash=%016llx ok=%d\n",
                label, (unsigned long long)diagnostics->elevation_ms,
                (unsigned long long)diagnostics->mountain_ms,
                (unsigned long long)diagnostics->climate_ms,
                (unsigned long long)diagnostics->hydrology_ms,
                (unsigned long long)diagnostics->classification_ms,
                (unsigned long long)diagnostics->total_ms,
                (unsigned long long)diagnostics->context_bytes,
                (unsigned long long)diagnostics->hydrology_bytes,
                (unsigned long long)diagnostics->staged_path_bytes,
                (unsigned long long)diagnostics->peak_bytes,
                (unsigned long long)diagnostics->physical_hash, timing_ok);
        ok &= timing_ok;
    }
    return ok;
}

static int run_case(FILE *file, const char *label, int map_size,
                    const WorldGenConfig *config, int strict, int decay, int save,
                    uint64_t *out_hash) {
    WorldGenContext *context;
    int ok;
    set_active_map_size(map_size);
    context = world_gen_prepare_with_config(config);
    if (!context) {
        const WorldGenDiagnostics *diagnostics = world_gen_last_diagnostics();
        int truncated = diagnostics->river_segments_required -
                        diagnostics->river_segments_copied;
        fprintf(file, "case=physical_prepare label=%s required_paths=%d copied_paths=%d "
                      "truncated=%d count_valid=%d ok=0\n",
                label, diagnostics->river_segments_required, diagnostics->river_segments_copied,
                truncated, river_path_count_valid(diagnostics->river_segments_required,
                                                  MAP_W, MAP_H));
        return 0;
    }
    if (out_hash) *out_hash = world_gen_last_diagnostics()->physical_hash;
    ok = game_worldgen_physical_probe_check_context(file, label, context, strict, save);
    ok &= game_worldgen_hydrology_probe_check_context(file, label, context, decay);
    world_gen_release_prepared(context);
    fprintf(file, "case=physical_prepare label=%s size=%d map=%dx%d ok=%d\n",
            label, map_size, MAP_W, MAP_H, ok);
    return ok;
}

int game_worldgen_physical_probe_run_matrix(FILE *file, const WorldGenConfig *base_config) {
    WorldGenConfig config;
    uint64_t hash_a1 = 0;
    uint64_t hash_a2 = 0;
    uint64_t hash_b = 0;
    uint64_t hash_a3 = 0;
    int levels[3] = {20, 50, 80};
    int ok = 1;
    int relief_index;
    int bias_index;
    if (!file || !base_config) return 0;
    ok &= game_worldgen_climate_probe_transport_contract(file);
    ok &= check_climate_repeat(file, base_config);
    config = *base_config;
    ok &= run_case(file, "determinism_a1", MAP_SIZE_SMALL, &config, 0, 1, 1, &hash_a1);
    ok &= run_case(file, "determinism_a2", MAP_SIZE_SMALL, &config, 0, 0, 0, &hash_a2);
    config.seed = base_config->seed + 1u;
    ok &= run_case(file, "determinism_b", MAP_SIZE_SMALL, &config, 0, 0, 0, &hash_b);
    config = *base_config;
    ok &= run_case(file, "determinism_a3", MAP_SIZE_SMALL, &config, 0, 0, 0, &hash_a3);
    {
        int deterministic = hash_a1 != 0 && hash_a1 == hash_a2 && hash_a1 == hash_a3 && hash_a1 != hash_b;
        fprintf(file, "case=physical_determinism fresh=%d aba=%d distinct_b=%d "
                      "a=%016llx b=%016llx ok=%d\n",
                hash_a1 == hash_a2, hash_a1 == hash_a3, hash_a1 != hash_b,
                (unsigned long long)hash_a1, (unsigned long long)hash_b, deterministic);
        ok &= deterministic;
    }
    for (relief_index = 0; relief_index < 3; relief_index++) {
        for (bias_index = 0; bias_index < 3; bias_index++) {
            char label[64];
            config = *base_config;
            config.relief = levels[relief_index];
            config.bias_mountain = levels[bias_index];
            snprintf(label, sizeof(label), "matrix_r%d_b%d", config.relief, config.bias_mountain);
            ok &= run_case(file, label, MAP_SIZE_SMALL, &config,
                           relief_index == 2 && bias_index == 2, 0, 0, NULL);
        }
    }
    config = *base_config;
    ok &= run_case(file, "map_size_medium", MAP_SIZE_MEDIUM, &config, 0, 0, 0, NULL);
    ok &= run_case(file, "map_size_large", MAP_SIZE_LARGE, &config, 0, 0, 0, NULL);
    fprintf(file, "case=physical_matrix small=1 medium=1 large=1 extreme=pending ok=%d\n", ok);
    return ok;
}
