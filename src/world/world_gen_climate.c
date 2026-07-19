#include "world_gen_climate.h"

#include "world/noise.h"
#include "world/wind_vector.h"
#include "world/world_gen_moisture.h"
#include "world/world_gen_rng.h"

#include <stdlib.h>

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int sign_int(int value) {
    return (value > 0) - (value < 0);
}

static int signed_latitude(const WorldGenContext *context, int x, int y, int axis) {
    int horizontal = (x * 200 / (context->width > 1 ? context->width - 1 : 1)) - 100;
    int vertical = (y * 200 / (context->height > 1 ? context->height - 1 : 1)) - 100;
    if (axis == 1) return horizontal;
    if (axis == 2) return clamp_int((horizontal + vertical) * 7 / 10, -100, 100);
    if (axis == 3) return clamp_int((horizontal - vertical) * 7 / 10, -100, 100);
    return vertical;
}

static void latitude_normal(int axis, int *out_x, int *out_y) {
    static const int normals[4][2] = {{0, 1024}, {1024, 0}, {724, 724}, {724, -724}};
    *out_x = normals[axis][0];
    *out_y = normals[axis][1];
}

static void build_wind(WorldGenContext *context, int axis, int rotation, int meander_seed) {
    int normal_x;
    int normal_y;
    int y;
    int x;
    latitude_normal(axis, &normal_x, &normal_y);
    for (y = 0; y < context->height; y++) {
        for (x = 0; x < context->width; x++) {
            int index = world_gen_context_index(context, x, y);
            int latitude = signed_latitude(context, x, y, axis);
            int absolute_latitude = abs(latitude);
            int tangent_x = -normal_y;
            int tangent_y = normal_x;
            int zonal;
            int meridional = 0;
            int base_speed;
            int meander = world_fractal_noise(x * 3, y * 3, meander_seed) - 50;
            int cross = world_fractal_noise(x * 2 + 71, y * 2 + 37, meander_seed + 113) - 50;
            int vector_x;
            int vector_y;
            if (absolute_latitude < 30) {
                zonal = -rotation;
                meridional = -sign_int(latitude) * 28;
                base_speed = 55 + absolute_latitude / 2;
            } else if (absolute_latitude < 65) {
                zonal = rotation;
                meridional = sign_int(latitude) * 10;
                base_speed = 42 + (absolute_latitude - 30) / 2;
            } else {
                zonal = -rotation;
                meridional = -sign_int(latitude) * 12;
                base_speed = 34 + (absolute_latitude - 65);
            }
            vector_x = tangent_x * zonal + normal_x * meridional / 100;
            vector_y = tangent_y * zonal + normal_y * meridional / 100;
            vector_x += normal_x * meander / 155 + tangent_x * cross / 230;
            vector_y += normal_y * meander / 155 + tangent_y * cross / 230;
            context->wind_direction16[index] = (uint8_t)wind_vector_nearest16(vector_x, vector_y);
            context->wind_speed[index] = (uint8_t)clamp_int(base_speed + meander / 3 + cross / 5, 0, 100);
        }
    }
}

static void smooth_field(WorldGenContext *context, int16_t *field, int passes) {
    int pass;
    int *next = context->scratch_a;
    for (pass = 0; pass < passes; pass++) {
        int y;
        int x;
        for (y = 0; y < context->height; y++) {
            for (x = 0; x < context->width; x++) {
                int total = 0;
                int count = 0;
                int dy;
                int dx;
                int index = world_gen_context_index(context, x, y);
                for (dy = -1; dy <= 1; dy++) {
                    for (dx = -1; dx <= 1; dx++) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (!world_gen_context_in_bounds(context, nx, ny)) continue;
                        total += field[world_gen_context_index(context, nx, ny)];
                        count++;
                    }
                }
                next[index] = count > 0 ? total / count : field[index];
            }
        }
        for (x = 0; x < context->tile_count; x++) field[x] = (int16_t)next[x];
    }
}

static void build_temperature(WorldGenContext *context, int axis, int anomaly_seed) {
    int i;
    for (i = 0; i < context->tile_count; i++) {
        int x = i % context->width;
        int y = i / context->width;
        int latitude = abs(signed_latitude(context, x, y, axis));
        int anomaly = (world_fractal_noise(x * 2, y * 2, anomaly_seed) - 50) / 4;
        int value = 92 - latitude * 78 / 100 + anomaly;
        int moderation = context->land_mask[i]
            ? clamp_int(24 - context->ocean_distance[i], 0, 24) : 30;
        value -= context->relative_altitude[i] * 48 / 100;
        value = (value * (100 - moderation) + 55 * moderation) / 100;
        context->temperature[i] = (int16_t)clamp_int(value, 0, 100);
    }
}

int world_gen_build_climate_fields(WorldGenContext *context) {
    WorldGenRng rng;
    int axis;
    int rotation;
    int wind_seed;
    int moisture_seed;
    int temperature_seed;
    if (!context) return 0;
    world_gen_rng_init(&rng, context->phase_seed[WORLD_GEN_PHASE_CLIMATE]);
    axis = world_gen_rng_bounded(&rng, 4);
    rotation = world_gen_rng_bounded(&rng, 2) ? 1 : -1;
    wind_seed = (int)(world_gen_rng_next(&rng) % 1000000u);
    moisture_seed = (int)(world_gen_rng_next(&rng) % 1000000u);
    temperature_seed = (int)(world_gen_rng_next(&rng) % 1000000u);
    build_wind(context, axis, rotation, wind_seed);
    if (!world_gen_transport_moisture(context, moisture_seed)) return 0;
    build_temperature(context, axis, temperature_seed);
    smooth_field(context, context->precipitation, 1);
    smooth_field(context, context->moisture, 2);
    smooth_field(context, context->temperature, 2);
    return 1;
}
