#include "world/world_gen_moisture.h"

#include "world/noise.h"
#include "world/wind_vector.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

enum {
    MOISTURE_VALUE_MASK = 255,
    MOISTURE_DISTANCE_SHIFT = 8,
    MOISTURE_DISTANCE_MAX = 0x7fffff,
    MOISTURE_ADVECTION_ROUNDS = 3,
    MOISTURE_SWEEP_COUNT = 4,
    MOISTURE_DIFFUSION_PASSES = 2
};

typedef struct {
    int value;
    int ocean_distance;
} MoistureSample;

static WorldGenMoistureDiagnostics last_diagnostics;

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int encoded_value(int encoded) {
    return encoded & MOISTURE_VALUE_MASK;
}

static int encoded_distance(int encoded) {
    return (int)((unsigned int)encoded >> MOISTURE_DISTANCE_SHIFT);
}

static int encode_moisture(int value, int ocean_distance) {
    return clamp_int(value, 0, 100) |
           (clamp_int(ocean_distance, 0, MOISTURE_DISTANCE_MAX) << MOISTURE_DISTANCE_SHIFT);
}

static int base_air_moisture(const WorldGenContext *context, int index, int moisture_seed) {
    int x = index % context->width;
    int y = index / context->width;
    int noise = world_fractal_noise(x, y, moisture_seed) - 50;
    if (!context->land_mask[index]) return 100;
    return clamp_int(18 + context->config.moisture / 2 - context->config.drought / 4 +
                     noise / 3, 4, 82);
}

static int bilinear_coordinate(int coordinate_q10, int limit, int *fraction_q10) {
    int maximum = (limit - 1) * WIND_VECTOR_SCALE;
    int base;
    coordinate_q10 = clamp_int(coordinate_q10, 0, maximum);
    base = coordinate_q10 / WIND_VECTOR_SCALE;
    *fraction_q10 = coordinate_q10 - base * WIND_VECTOR_SCALE;
    return base;
}

static MoistureSample sample_moisture(const WorldGenContext *context, const int32_t *field,
                                      int x_q10, int y_q10) {
    MoistureSample result = {0, 0};
    int fraction_x;
    int fraction_y;
    int x0 = bilinear_coordinate(x_q10, context->width, &fraction_x);
    int y0 = bilinear_coordinate(y_q10, context->height, &fraction_y);
    int x1 = x0 + 1 < context->width ? x0 + 1 : x0;
    int y1 = y0 + 1 < context->height ? y0 + 1 : y0;
    int indices[4] = {y0 * context->width + x0, y0 * context->width + x1,
                      y1 * context->width + x0, y1 * context->width + x1};
    int weights[4] = {
        (WIND_VECTOR_SCALE - fraction_x) * (WIND_VECTOR_SCALE - fraction_y),
        fraction_x * (WIND_VECTOR_SCALE - fraction_y),
        (WIND_VECTOR_SCALE - fraction_x) * fraction_y,
        fraction_x * fraction_y
    };
    int64_t weighted_value = 0;
    int nearest_ocean = INT_MAX;
    int i;
    for (i = 0; i < 4; i++) {
        int distance;
        if (weights[i] <= 0) continue;
        weighted_value += (int64_t)encoded_value(field[indices[i]]) * weights[i];
        distance = encoded_distance(field[indices[i]]);
        if (distance > 0 && distance < nearest_ocean) nearest_ocean = distance;
    }
    result.value = (int)((weighted_value + (INT64_C(1) << 19)) >> 20);
    result.ocean_distance = nearest_ocean == INT_MAX ? 0 : nearest_ocean;
    return result;
}

static int sample_elevation(const WorldGenContext *context, int x_q10, int y_q10) {
    int fraction_x;
    int fraction_y;
    int x0 = bilinear_coordinate(x_q10, context->width, &fraction_x);
    int y0 = bilinear_coordinate(y_q10, context->height, &fraction_y);
    int x1 = x0 + 1 < context->width ? x0 + 1 : x0;
    int y1 = y0 + 1 < context->height ? y0 + 1 : y0;
    int64_t top = (int64_t)context->elevation[y0 * context->width + x0] *
                  (WIND_VECTOR_SCALE - fraction_x) +
                  (int64_t)context->elevation[y0 * context->width + x1] * fraction_x;
    int64_t bottom = (int64_t)context->elevation[y1 * context->width + x0] *
                     (WIND_VECTOR_SCALE - fraction_x) +
                     (int64_t)context->elevation[y1 * context->width + x1] * fraction_x;
    return (int)((top * (WIND_VECTOR_SCALE - fraction_y) + bottom * fraction_y +
                  (INT64_C(1) << 19)) >> 20);
}

static int transfer_moisture(const WorldGenContext *context, int index,
                             MoistureSample incoming_sample, int upstream_elevation,
                             int speed, int *out_precipitation) {
    int incoming = incoming_sample.value;
    int ambient = context->moisture[index];
    int advection_weight = 448 + speed * 5;
    int carried = (incoming * advection_weight + ambient * (WIND_VECTOR_SCALE - advection_weight) +
                   WIND_VECTOR_SCALE / 2) / WIND_VECTOR_SCALE;
    int rise = context->elevation[index] - upstream_elevation;
    int descent = -rise;
    int precipitation = 0;
    int orographic_precipitation = 0;
    int lee_drying = 0;
    int coast_recharge = 0;
    int result;

    if (rise > 0) {
        int relief_factor = 3 + context->config.relief / 40;
        orographic_precipitation = rise * relief_factor * (70 + speed) / 140;
        precipitation += orographic_precipitation;
    }
    if (carried > 64) precipitation += (carried - 64) * (80 + speed) / 420;
    precipitation = clamp_int(precipitation, 0, 36);
    if (descent > 0) lee_drying = clamp_int(descent * (35 + speed) / 70, 0, 18);
    if (context->ocean_distance[index] <= 4) {
        coast_recharge = (5 - context->ocean_distance[index]) * 3;
    }
    result = carried - precipitation - lee_drying + coast_recharge;
    if (result < ambient) {
        int recovery_weight = 120 - speed / 2;
        result += (ambient - result) * recovery_weight / WIND_VECTOR_SCALE;
    }
    *out_precipitation = clamp_int(precipitation * 100 / 28, 0, 100);
    last_diagnostics.advection_weight_total += (uint64_t)advection_weight;
    last_diagnostics.orographic_precipitation_total +=
        (uint64_t)orographic_precipitation;
    last_diagnostics.lee_drying_total += (uint64_t)lee_drying;
    return encode_moisture(result,
                           incoming_sample.ocean_distance > 0
                               ? incoming_sample.ocean_distance + 1 : 0);
}

static void update_advection_tile(WorldGenContext *context, int index) {
    int x = index % context->width;
    int y = index / context->width;
    int speed = clamp_int(context->wind_speed[index], 0, 100);
    int travel_q10 = 640 + speed * 6;
    int upstream_x_q10;
    int upstream_y_q10;
    int precipitation;
    MoistureSample incoming;
    int upstream_elevation;
    wind_vector_offset_point_q10(context->wind_direction16[index] & 15, x, y,
                                 -travel_q10, 0, &upstream_x_q10, &upstream_y_q10);
    incoming = sample_moisture(context, context->scratch_a, upstream_x_q10, upstream_y_q10);
    upstream_elevation = sample_elevation(context, upstream_x_q10, upstream_y_q10);
    context->scratch_a[index] = transfer_moisture(
        context, index, incoming, upstream_elevation, speed, &precipitation);
    if (precipitation > context->precipitation[index]) {
        context->precipitation[index] = (int16_t)precipitation;
    }
    last_diagnostics.subtile_advection_samples++;
}

static void run_advection_sweep(WorldGenContext *context, int sweep_class) {
    int x_start = (sweep_class & 1) ? context->width - 1 : 0;
    int x_end = (sweep_class & 1) ? -1 : context->width;
    int x_step = (sweep_class & 1) ? -1 : 1;
    int y_start = (sweep_class & 2) ? context->height - 1 : 0;
    int y_end = (sweep_class & 2) ? -1 : context->height;
    int y_step = (sweep_class & 2) ? -1 : 1;
    int y;
    for (y = y_start; y != y_end; y += y_step) {
        int x;
        for (x = x_start; x != x_end; x += x_step) {
            int index = world_gen_context_index(context, x, y);
            if (!context->land_mask[index] ||
                wind_vector_sweep_class(context->wind_direction16[index] & 15) != sweep_class) continue;
            update_advection_tile(context, index);
        }
    }
}

static void solve_advection(WorldGenContext *context) {
    int round;
    for (round = 0; round < MOISTURE_ADVECTION_ROUNDS; round++) {
        int sweep;
        for (sweep = 0; sweep < MOISTURE_SWEEP_COUNT; sweep++) {
            run_advection_sweep(context, sweep);
        }
    }
    last_diagnostics.advection_rounds = MOISTURE_ADVECTION_ROUNDS;
}

static void diffuse_moisture(WorldGenContext *context) {
    int pass;
    for (pass = 0; pass < MOISTURE_DIFFUSION_PASSES; pass++) {
        int i;
        for (i = 0; i < context->tile_count; i++) {
            int current_distance = encoded_distance(context->scratch_a[i]);
            if (!context->land_mask[i]) {
                context->scratch_b[i] = encode_moisture(100, 1);
                continue;
            }
            {
                int x = i % context->width;
                int y = i / context->width;
                int speed = clamp_int(context->wind_speed[i], 0, 100);
                int left_x_q10, left_y_q10, right_x_q10, right_y_q10;
                int mix_weight = 224 - speed;
                MoistureSample left;
                MoistureSample right;
                int crosswind;
                int value;
                int distance = current_distance;
                wind_vector_offset_point_q10(context->wind_direction16[i] & 15, x, y, 0,
                                             -WIND_VECTOR_SCALE,
                                             &left_x_q10, &left_y_q10);
                wind_vector_offset_point_q10(context->wind_direction16[i] & 15, x, y, 0,
                                             WIND_VECTOR_SCALE,
                                             &right_x_q10, &right_y_q10);
                left = sample_moisture(context, context->scratch_a, left_x_q10, left_y_q10);
                right = sample_moisture(context, context->scratch_a, right_x_q10, right_y_q10);
                crosswind = (left.value + right.value + 1) / 2;
                value = (encoded_value(context->scratch_a[i]) *
                         (WIND_VECTOR_SCALE - mix_weight) + crosswind * mix_weight +
                         WIND_VECTOR_SCALE / 2) / WIND_VECTOR_SCALE;
                if (distance == 0 || (left.ocean_distance > 0 && left.ocean_distance < distance)) {
                    distance = left.ocean_distance;
                }
                if (distance == 0 || (right.ocean_distance > 0 && right.ocean_distance < distance)) {
                    distance = right.ocean_distance;
                }
                context->scratch_b[i] = encode_moisture(value, distance > 0 ? distance + 1 : 0);
                last_diagnostics.lateral_mix_weight_total += (uint64_t)mix_weight;
                last_diagnostics.subtile_lateral_samples += 2;
            }
        }
        memcpy(context->scratch_a, context->scratch_b,
               (size_t)context->tile_count * sizeof(*context->scratch_a));
    }
}

static void finalize_fields(WorldGenContext *context, int moisture_seed) {
    int i;
    for (i = 0; i < context->tile_count; i++) {
        int x = i % context->width;
        int y = i / context->width;
        int noise = world_fractal_noise(x, y, moisture_seed + 337) - 50;
        int coast = context->land_mask[i]
            ? clamp_int(22 - context->ocean_distance[i], 0, 22) : 35;
        int transported = encoded_value(context->scratch_a[i]);
        int rain = context->precipitation[i];
        int ocean_chain = encoded_distance(context->scratch_a[i]);
        if (!context->land_mask[i]) {
            context->moisture[i] = 100;
            context->precipitation[i] = 0;
            continue;
        }
        context->moisture[i] = (int16_t)clamp_int(
            context->config.moisture / 3 - context->config.drought / 5 +
            transported / 2 + rain / 2 + coast + noise / 4, 0, 100);
        if (ocean_chain > 0) {
            int length = ocean_chain - 1;
            last_diagnostics.ocean_reached_land_tiles++;
            if (length > 20) last_diagnostics.ocean_reached_beyond_20++;
            if (length > last_diagnostics.max_ocean_chain_length) {
                last_diagnostics.max_ocean_chain_length = length;
            }
        }
    }
}

int world_gen_transport_moisture(WorldGenContext *context, int moisture_seed) {
    int i;
    int land_tiles = 0;
    if (!context || context->tile_count <= 0) return 0;
    memset(&last_diagnostics, 0, sizeof(last_diagnostics));
    last_diagnostics.tile_count = context->tile_count;
    last_diagnostics.climate_seed = context->phase_seed[WORLD_GEN_PHASE_CLIMATE];
    last_diagnostics.diffusion_passes = MOISTURE_DIFFUSION_PASSES;
    for (i = 0; i < context->tile_count; i++) {
        int base = base_air_moisture(context, i, moisture_seed);
        context->moisture[i] = (int16_t)base;
        context->precipitation[i] = 0;
        if (context->land_mask[i]) {
            context->scratch_a[i] = encode_moisture(base, 0);
            land_tiles++;
        } else {
            context->scratch_a[i] = encode_moisture(100, 1);
        }
    }
    solve_advection(context);
    diffuse_moisture(context);
    finalize_fields(context, moisture_seed);
    last_diagnostics.solved_land_tiles = land_tiles;
    return land_tiles > 0;
}

const WorldGenMoistureDiagnostics *world_gen_moisture_last_diagnostics(void) {
    return &last_diagnostics;
}
