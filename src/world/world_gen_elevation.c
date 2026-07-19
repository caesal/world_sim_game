#include "world_gen_elevation.h"

#include "world/noise.h"
#include "world/world_gen_land_mask.h"
#include "world/world_gen_rng.h"

#include <stdlib.h>

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static void smooth_elevation(WorldGenContext *context, int passes) {
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
                        total += context->elevation[world_gen_context_index(context, nx, ny)];
                        count++;
                    }
                }
                next[index] = count > 0 ? total / count : context->elevation[index];
            }
        }
        for (x = 0; x < context->tile_count; x++) context->elevation[x] = (int16_t)next[x];
    }
}

static void compute_terrain_derivatives(WorldGenContext *context) {
    int y;
    int x;
    for (y = 0; y < context->height; y++) {
        for (x = 0; x < context->width; x++) {
            int index = world_gen_context_index(context, x, y);
            int maximum_delta = 0;
            int neighbor_total = 0;
            int neighbor_count = 0;
            int dy;
            int dx;
            for (dy = -1; dy <= 1; dy++) {
                for (dx = -1; dx <= 1; dx++) {
                    int next;
                    int delta;
                    if ((dx == 0 && dy == 0) || !world_gen_context_in_bounds(context, x + dx, y + dy)) continue;
                    next = world_gen_context_index(context, x + dx, y + dy);
                    delta = abs((int)context->elevation[index] - (int)context->elevation[next]);
                    if (delta > maximum_delta) maximum_delta = delta;
                    neighbor_total += context->elevation[next];
                    neighbor_count++;
                }
            }
            context->relative_altitude[index] = context->land_mask[index]
                ? (int16_t)(context->elevation[index] - context->sea_level) : 0;
            context->slope[index] = (int16_t)maximum_delta;
            context->curvature[index] = neighbor_count > 0
                ? (int16_t)(context->elevation[index] - neighbor_total / neighbor_count) : 0;
        }
    }
}

int world_gen_build_elevation_and_mask(WorldGenContext *context) {
    WorldGenRng rng;
    int elevation_seed;
    int fragment_seed;
    int edge_mode;
    int edge_strength;
    int edge_axis;
    int edge_sign;
    int y;
    int x;

    if (!context) return 0;
    world_gen_rng_init(&rng, context->phase_seed[WORLD_GEN_PHASE_ELEVATION]);
    elevation_seed = (int)(world_gen_rng_next(&rng) % 1000000u);
    fragment_seed = (int)(world_gen_rng_next(&rng) % 1000000u);
    edge_mode = world_gen_rng_bounded(&rng, 4);
    edge_strength = world_gen_rng_bounded(&rng, 7);
    edge_axis = world_gen_rng_bounded(&rng, 2);
    edge_sign = world_gen_rng_bounded(&rng, 2) ? 1 : -1;

    for (y = 0; y < context->height; y++) {
        for (x = 0; x < context->width; x++) {
            int index = world_gen_context_index(context, x, y);
            int edge_x = x < context->width - 1 - x ? x : context->width - 1 - x;
            int edge_y = y < context->height - 1 - y ? y : context->height - 1 - y;
            int edge = edge_x < edge_y ? edge_x : edge_y;
            int max_edge = (context->width < context->height ? context->width : context->height) / 2;
            int edge_factor = max_edge > 0 ? edge * 100 / max_edge : 50;
            int axis_pos = edge_axis == 0 ? x * 100 / (context->width > 1 ? context->width - 1 : 1)
                                          : y * 100 / (context->height > 1 ? context->height - 1 : 1);
            int edge_bias = 0;
            int broad = world_fractal_noise(x, y, elevation_seed);
            int broken = world_fractal_noise(x * 2 + elevation_seed % 97,
                                             y * 2 + elevation_seed % 53, fragment_seed);
            int fragment = clamp_int(context->config.continent, 0, 100);
            if (edge_mode == 1) edge_bias = (edge_factor - 50) * edge_strength / 50;
            else if (edge_mode == 2) edge_bias = (50 - edge_factor) * edge_strength / 50;
            else if (edge_mode == 3) edge_bias = (axis_pos - 50) * edge_strength * edge_sign / 50;
            context->elevation[index] = (int16_t)clamp_int(
                (broad * (100 - fragment) + broken * fragment) / 100 +
                clamp_int(edge_bias, -6, 6) + (context->config.relief - 50) / 5, 0, 100);
        }
    }
    smooth_elevation(context, clamp_int(7 - context->config.continent / 20, 2, 7));
    if (!world_gen_land_mask_build(context)) return 0;
    world_gen_finalize_elevation(context);
    return 1;
}

void world_gen_finalize_elevation(WorldGenContext *context) {
    int i;
    if (!context) return;
    for (i = 0; i < context->tile_count; i++) {
        int value = context->elevation[i];
        if (context->land_mask[i]) value = clamp_int(value, context->sea_level + 1, 100);
        else value = clamp_int(value, 0, context->sea_level - 1);
        context->elevation[i] = (int16_t)value;
    }
    world_gen_land_mask_refresh_ocean_distance(context);
    compute_terrain_derivatives(context);
}
