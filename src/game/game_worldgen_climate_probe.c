#include "game/game_worldgen_climate_probe.h"

#include "world/wind_vector.h"
#include "world/world_gen_moisture.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int axis;
    int rotation;
    int matches[3];
    int totals[3];
    int meridional_matches[3];
    int meridional_totals[3];
    int score;
} WindBandFit;

static int sign_int(int value) {
    return (value > 0) - (value < 0);
}

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int signed_latitude(const WorldGenContext *context, int x, int y, int axis) {
    int horizontal = x * 200 / (context->width > 1 ? context->width - 1 : 1) - 100;
    int vertical = y * 200 / (context->height > 1 ? context->height - 1 : 1) - 100;
    if (axis == 1) return horizontal;
    if (axis == 2) return clamp_int((horizontal + vertical) * 7 / 10, -100, 100);
    if (axis == 3) return clamp_int((horizontal - vertical) * 7 / 10, -100, 100);
    return vertical;
}

static void latitude_normal(int axis, int *normal_x, int *normal_y) {
    static const int normals[4][2] = {{0, 1024}, {1024, 0}, {724, 724}, {724, -724}};
    *normal_x = normals[axis][0];
    *normal_y = normals[axis][1];
}

static int latitude_band(int absolute_latitude) {
    if (absolute_latitude < 30) return 0;
    if (absolute_latitude < 65) return 1;
    return 2;
}

static WindBandFit fit_candidate(const WorldGenContext *context, int axis, int rotation) {
    WindBandFit fit;
    int normal_x;
    int normal_y;
    int tangent_x;
    int tangent_y;
    int stride = context->tile_count / 16000;
    int i;
    memset(&fit, 0, sizeof(fit));
    fit.axis = axis;
    fit.rotation = rotation;
    if (stride < 1) stride = 1;
    latitude_normal(axis, &normal_x, &normal_y);
    tangent_x = -normal_y;
    tangent_y = normal_x;
    for (i = 0; i < context->tile_count; i += stride) {
        int x = i % context->width;
        int y = i / context->width;
        int latitude = signed_latitude(context, x, y, axis);
        int absolute_latitude = abs(latitude);
        int band = latitude_band(absolute_latitude);
        int direction = context->wind_direction16[i] & 15;
        WindVectorQ10 vector;
        int zonal;
        int meridional;
        int expected_zonal = band == 1 ? rotation : -rotation;
        int expected_meridional = band == 1 ? sign_int(latitude) : -sign_int(latitude);
        wind_vector_get_q10(direction, &vector);
        zonal = vector.x_q10 * tangent_x + vector.y_q10 * tangent_y;
        meridional = vector.x_q10 * normal_x + vector.y_q10 * normal_y;
        fit.totals[band]++;
        if (zonal * expected_zonal > 0) fit.matches[band]++;
        if (absolute_latitude >= 5) {
            fit.meridional_totals[band]++;
            if (meridional * expected_meridional > 0) fit.meridional_matches[band]++;
        }
    }
    for (i = 0; i < 3; i++) {
        fit.score += fit.matches[i] * 4 + fit.meridional_matches[i];
    }
    return fit;
}

static WindBandFit infer_wind_bands(const WorldGenContext *context) {
    WindBandFit best;
    int axis;
    int rotation;
    memset(&best, 0, sizeof(best));
    best.score = INT_MIN;
    for (axis = 0; axis < 4; axis++) {
        for (rotation = -1; rotation <= 1; rotation += 2) {
            WindBandFit candidate = fit_candidate(context, axis, rotation);
            if (candidate.score > best.score) best = candidate;
        }
    }
    return best;
}

static int check_wind(FILE *file, const char *label, const WorldGenContext *context) {
    int direction_counts[16] = {0};
    int bounds_errors = 0;
    int distinct = 0;
    int minimum_speed = 101;
    int maximum_speed = -1;
    WindBandFit fit;
    int band_ok = 1;
    int i;
    for (i = 0; i < context->tile_count; i++) {
        int direction = context->wind_direction16[i];
        int speed = context->wind_speed[i];
        if (direction < 0 || direction >= 16 || speed < 0 || speed > 100) bounds_errors++;
        else direction_counts[direction]++;
        if (speed < minimum_speed) minimum_speed = speed;
        if (speed > maximum_speed) maximum_speed = speed;
    }
    for (i = 0; i < 16; i++) distinct += direction_counts[i] > 0;
    fit = infer_wind_bands(context);
    for (i = 0; i < 3; i++) {
        if (fit.totals[i] <= 0 || fit.matches[i] * 100 < fit.totals[i] * 75) band_ok = 0;
    }
    fprintf(file, "case=climate_wind label=%s bounds=%d distinct=%d speed=%d..%d "
                  "axis=%d rotation=%d tropical=%d/%d mid=%d/%d polar=%d/%d "
                  "meridional=%d/%d,%d/%d,%d/%d ok=%d\n",
            label, bounds_errors, distinct, minimum_speed, maximum_speed,
            fit.axis, fit.rotation, fit.matches[0], fit.totals[0],
            fit.matches[1], fit.totals[1], fit.matches[2], fit.totals[2],
            fit.meridional_matches[0], fit.meridional_totals[0],
            fit.meridional_matches[1], fit.meridional_totals[1],
            fit.meridional_matches[2], fit.meridional_totals[2],
            bounds_errors == 0 && distinct >= 4 && maximum_speed - minimum_speed >= 8 && band_ok);
    return bounds_errors == 0 && distinct >= 4 && maximum_speed - minimum_speed >= 8 && band_ok;
}

static int check_ocean_reach(FILE *file, const char *label,
                             const WorldGenContext *context, int require_full_domain) {
    const WorldGenMoistureDiagnostics *diagnostics = world_gen_moisture_last_diagnostics();
    int land = 0;
    int ocean = 0;
    int metadata_ok;
    int reach_ok;
    int i;
    for (i = 0; i < context->tile_count; i++) {
        if (context->land_mask[i]) land++;
        else ocean++;
    }
    metadata_ok = diagnostics && diagnostics->tile_count == context->tile_count &&
                   diagnostics->climate_seed == context->phase_seed[WORLD_GEN_PHASE_CLIMATE] &&
                   diagnostics->solved_land_tiles == land && diagnostics->diffusion_passes == 2 &&
                   diagnostics->advection_rounds == 3 &&
                   diagnostics->subtile_advection_samples == land * 3 &&
                   diagnostics->subtile_lateral_samples == land * 4 &&
                   diagnostics->unconverged_cycles == 0 &&
                   diagnostics->max_cycle_iterations <= 32;
    if (ocean == 0) {
        reach_ok = diagnostics && diagnostics->ocean_reached_land_tiles == 0;
    } else if (require_full_domain) {
        reach_ok = diagnostics && diagnostics->max_ocean_chain_length > 20 &&
                   diagnostics->ocean_reached_beyond_20 > 0;
    } else {
        reach_ok = diagnostics && diagnostics->ocean_reached_land_tiles > 0;
    }
    fprintf(file, "case=climate_ocean_reach label=%s land=%d ocean=%d solved=%d "
                  "cycle_tiles=%d cycles=%d iterations=%d max_iteration=%d unconverged=%d "
                  "reached=%d beyond20=%d max_chain=%d diffusion=%d advection=%d "
                  "subtile=%d/%d required=%d ok=%d\n",
            label, land, ocean, diagnostics ? diagnostics->solved_land_tiles : -1,
            diagnostics ? diagnostics->cycle_tiles : -1,
            diagnostics ? diagnostics->cycle_count : -1,
            diagnostics ? diagnostics->cycle_iterations : -1,
            diagnostics ? diagnostics->max_cycle_iterations : -1,
            diagnostics ? diagnostics->unconverged_cycles : -1,
            diagnostics ? diagnostics->ocean_reached_land_tiles : -1,
            diagnostics ? diagnostics->ocean_reached_beyond_20 : -1,
            diagnostics ? diagnostics->max_ocean_chain_length : -1,
            diagnostics ? diagnostics->diffusion_passes : -1,
            diagnostics ? diagnostics->advection_rounds : -1,
            diagnostics ? diagnostics->subtile_advection_samples : -1,
            diagnostics ? diagnostics->subtile_lateral_samples : -1,
            require_full_domain, metadata_ok && reach_ok);
    return metadata_ok && reach_ok;
}

static void direction_offset(int direction, int x, int y, int distance,
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

static int check_ridge_pairs(FILE *file, const char *label,
                             const WorldGenContext *context, int required) {
    uint64_t windward_rain = 0;
    uint64_t leeward_rain = 0;
    uint64_t leeward_moisture = 0;
    uint64_t recovery_moisture = 0;
    int stride = context->tile_count / 50000;
    int pairs = 0;
    int i;
    if (stride < 1) stride = 1;
    for (i = 0; i < context->tile_count; i += stride) {
        int x;
        int y;
        int up_x;
        int up_y;
        int lee_x;
        int lee_y;
        int recovery_x;
        int recovery_y;
        int up;
        int lee;
        int recovery;
        int windward;
        if (context->mountain_uplift[i] < 18) continue;
        x = i % context->width;
        y = i / context->width;
        direction_offset(context->wind_direction16[i], x, y, -2, &up_x, &up_y);
        direction_offset(context->wind_direction16[i], x, y, 3, &lee_x, &lee_y);
        direction_offset(context->wind_direction16[i], x, y, 9,
                         &recovery_x, &recovery_y);
        if (!world_gen_context_in_bounds(context, up_x, up_y) ||
            !world_gen_context_in_bounds(context, lee_x, lee_y) ||
            !world_gen_context_in_bounds(context, recovery_x, recovery_y)) continue;
        up = world_gen_context_index(context, up_x, up_y);
        lee = world_gen_context_index(context, lee_x, lee_y);
        recovery = world_gen_context_index(context, recovery_x, recovery_y);
        if (!context->land_mask[up] || !context->land_mask[lee] ||
            !context->land_mask[recovery]) continue;
        if (context->elevation[i] < context->elevation[up] + 4 ||
            context->elevation[i] < context->elevation[lee] + 4) continue;
        windward = context->precipitation[i] > context->precipitation[up]
            ? context->precipitation[i] : context->precipitation[up];
        windward_rain += (uint16_t)windward;
        leeward_rain += (uint16_t)context->precipitation[lee];
        leeward_moisture += (uint16_t)context->moisture[lee];
        recovery_moisture += (uint16_t)context->moisture[recovery];
        pairs++;
    }
    {
        int contrast_ok = pairs >= 4 && windward_rain > leeward_rain;
        int recovery_ok = pairs >= 4 && recovery_moisture >= leeward_moisture;
        int ok = !required || (contrast_ok && recovery_ok);
        fprintf(file, "case=climate_ridge label=%s pairs=%d windward=%llu leeward=%llu "
                      "lee_moisture=%llu recovery=%llu required=%d ok=%d\n",
                label, pairs, (unsigned long long)windward_rain,
                (unsigned long long)leeward_rain,
                (unsigned long long)leeward_moisture,
                (unsigned long long)recovery_moisture, required, ok);
        return ok;
    }
}

static int check_diffusion(FILE *file, const char *label, const WorldGenContext *context) {
    int abrupt = 0;
    int pairs = 0;
    int y;
    int x;
    for (y = 0; y < context->height; y++) {
        for (x = 0; x < context->width; x++) {
            int index = world_gen_context_index(context, x, y);
            if (x + 1 < context->width) {
                abrupt += abs(context->moisture[index] - context->moisture[index + 1]) > 40;
                pairs++;
            }
            if (y + 1 < context->height) {
                abrupt += abs(context->moisture[index] -
                              context->moisture[index + context->width]) > 40;
                pairs++;
            }
        }
    }
    fprintf(file, "case=climate_diffusion label=%s abrupt=%d pairs=%d ok=%d\n",
            label, abrupt, pairs, pairs > 0 && abrupt * 50 <= pairs);
    return pairs > 0 && abrupt * 50 <= pairs;
}

uint64_t game_worldgen_climate_probe_hash(const WorldGenContext *context) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int i;
    if (!context) return 0;
    for (i = 0; i < context->tile_count; i++) {
        uint64_t value = context->wind_direction16[i] |
                         ((uint64_t)context->wind_speed[i] << 8) |
                         ((uint64_t)(uint16_t)context->moisture[i] << 16) |
                         ((uint64_t)(uint16_t)context->temperature[i] << 32) |
                         ((uint64_t)(uint16_t)context->precipitation[i] << 48);
        int byte_index;
        for (byte_index = 0; byte_index < 8; byte_index++) {
            hash ^= (uint8_t)(value >> (byte_index * 8));
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

static uint64_t transport_output_hash(const WorldGenContext *context) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int i;
    for (i = 0; i < context->tile_count; i++) {
        uint32_t value = (uint16_t)context->moisture[i] |
                         ((uint32_t)(uint16_t)context->precipitation[i] << 16);
        int byte_index;
        for (byte_index = 0; byte_index < 4; byte_index++) {
            hash ^= (uint8_t)(value >> (byte_index * 8));
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

static int run_transport_fixture(int direction, int speed, uint64_t *out_hash,
                                 WorldGenMoistureDiagnostics *out_diagnostics) {
    WorldGenContext context;
    WorldGenConfig config;
    int ok;
    int y;
    int x;
    memset(&context, 0, sizeof(context));
    memset(&config, 0, sizeof(config));
    config.moisture = 50;
    config.drought = 50;
    config.relief = 70;
    if (!world_gen_context_create(&context, &config, 64, 32, UINT32_C(0x51a7c19d))) return 0;
    for (y = 0; y < context.height; y++) {
        for (x = 0; x < context.width; x++) {
            int index = world_gen_context_index(&context, x, y);
            int elevation = 20;
            if (x >= 27 && x <= 30) elevation = 80;
            else if (x == 26 || x == 31) elevation = 45;
            context.land_mask[index] = (uint8_t)(x >= 4);
            context.elevation[index] = (int16_t)elevation;
            context.ocean_distance[index] = (int16_t)(x >= 4 ? x - 3 : 0);
            context.wind_direction16[index] = (uint8_t)direction;
            context.wind_speed[index] = (uint8_t)speed;
        }
    }
    ok = world_gen_transport_moisture(&context, 77123);
    if (ok && out_hash) *out_hash = transport_output_hash(&context);
    if (ok && out_diagnostics) *out_diagnostics = *world_gen_moisture_last_diagnostics();
    world_gen_context_destroy(&context);
    return ok;
}

int game_worldgen_climate_probe_transport_contract(FILE *file) {
    WorldGenMoistureDiagnostics low = {0};
    WorldGenMoistureDiagnostics high = {0};
    uint64_t low_hash = 0;
    uint64_t high_hash = 0;
    uint64_t east_hash = 0;
    uint64_t oblique_hash = 0;
    uint64_t repeat_hash = 0;
    int vectors_ok = 1;
    int direction;
    int runs_ok;
    int speed_ok;
    int heading_ok;
    int ok;
    if (!file) return 0;
    for (direction = 0; direction < WIND_VECTOR_DIRECTION_COUNT; direction++) {
        WindVectorQ10 vector;
        int64_t magnitude_squared;
        vectors_ok &= wind_vector_get_q10(direction, &vector);
        magnitude_squared = (int64_t)vector.x_q10 * vector.x_q10 +
                            (int64_t)vector.y_q10 * vector.y_q10;
        vectors_ok &= magnitude_squared >= INT64_C(1020) * 1020 &&
                      magnitude_squared <= INT64_C(1028) * 1028;
        vectors_ok &= wind_vector_nearest16(vector.x_q10, vector.y_q10) == direction;
        vectors_ok &= wind_vector_sweep_class(direction) >= 0 &&
                      wind_vector_sweep_class(direction) < 4;
    }
    runs_ok = run_transport_fixture(1, 15, &low_hash, &low) &&
              run_transport_fixture(1, 90, &high_hash, &high) &&
              run_transport_fixture(0, 55, &east_hash, NULL) &&
              run_transport_fixture(1, 55, &oblique_hash, NULL) &&
              run_transport_fixture(1, 55, &repeat_hash, NULL);
    speed_ok = runs_ok && low_hash != high_hash &&
               high.advection_weight_total > low.advection_weight_total &&
               low.lateral_mix_weight_total > high.lateral_mix_weight_total &&
               high.orographic_precipitation_total > low.orographic_precipitation_total &&
               high.lee_drying_total > low.lee_drying_total &&
               low.subtile_advection_samples > 0 && low.subtile_lateral_samples > 0;
    heading_ok = runs_ok && east_hash != oblique_hash && oblique_hash == repeat_hash;
    ok = vectors_ok && speed_ok && heading_ok;
    fprintf(file,
            "case=climate_transport_contract vectors16=%d runs=%d speed_hash=%d "
            "advection=%llu/%llu lateral=%llu/%llu orographic=%llu/%llu "
            "lee=%llu/%llu heading_distinct=%d repeat=%d samples=%d/%d ok=%d\n",
            vectors_ok, runs_ok, low_hash != high_hash,
            (unsigned long long)low.advection_weight_total,
            (unsigned long long)high.advection_weight_total,
            (unsigned long long)low.lateral_mix_weight_total,
            (unsigned long long)high.lateral_mix_weight_total,
            (unsigned long long)low.orographic_precipitation_total,
            (unsigned long long)high.orographic_precipitation_total,
            (unsigned long long)low.lee_drying_total,
            (unsigned long long)high.lee_drying_total,
            east_hash != oblique_hash, oblique_hash == repeat_hash,
            low.subtile_advection_samples, low.subtile_lateral_samples, ok);
    return ok;
}

int game_worldgen_climate_probe_compare(FILE *file, const char *label,
                                        const WorldGenContext *first,
                                        const WorldGenContext *second) {
    uint64_t first_hash = game_worldgen_climate_probe_hash(first);
    uint64_t second_hash = game_worldgen_climate_probe_hash(second);
    int dimensions_match = first && second && first->width == second->width &&
                           first->height == second->height;
    int ok = dimensions_match && first_hash != 0 && first_hash == second_hash;
    if (file) {
        fprintf(file, "case=climate_determinism label=%s first=%016llx second=%016llx "
                      "dimensions=%d ok=%d\n",
                label ? label : "repeat", (unsigned long long)first_hash,
                (unsigned long long)second_hash, dimensions_match, ok);
    }
    return ok;
}

int game_worldgen_climate_probe_check_context(FILE *file, const char *label,
                                               const WorldGenContext *context,
                                               int require_full_domain) {
    int ok;
    if (!file || !label || !context) return 0;
    ok = check_wind(file, label, context);
    ok &= check_ocean_reach(file, label, context, require_full_domain);
    ok &= check_ridge_pairs(file, label, context, require_full_domain);
    ok &= check_diffusion(file, label, context);
    return ok;
}
