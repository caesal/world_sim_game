#include "mountain_gen.h"

#include "world/world_gen_rng.h"

#include <stdlib.h>

static int last_chain_count;
static int last_average_length;

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int integer_sqrt(int value) {
    unsigned int bit = 1u << 30;
    unsigned int result = 0;
    unsigned int remainder = value > 0 ? (unsigned int)value : 0;
    while (bit > remainder) bit >>= 2;
    while (bit != 0) {
        if (remainder >= result + bit) {
            remainder -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return (int)result;
}

static int smooth_weight(int distance, int radius) {
    int t;
    if (radius <= 0 || distance >= radius) return 0;
    t = (radius - distance) * 1024 / radius;
    return t * t / 1024 * (3072 - 2 * t) / 1024;
}

static void apply_profile(WorldGenContext *context, int cx, int cy, int shoulder_radius,
                          int core_radius, int strength, int taper, int saddle) {
    int y;
    int x;
    for (y = cy - shoulder_radius; y <= cy + shoulder_radius; y++) {
        for (x = cx - shoulder_radius; x <= cx + shoulder_radius; x++) {
            int index;
            int dx;
            int dy;
            int distance;
            int shoulder;
            int core = 0;
            int lift;
            if (!world_gen_context_in_bounds(context, x, y)) continue;
            index = world_gen_context_index(context, x, y);
            if (!context->land_mask[index]) continue;
            dx = x - cx;
            dy = y - cy;
            distance = integer_sqrt(dx * dx + dy * dy);
            if (distance >= shoulder_radius) continue;
            shoulder = strength * 38 / 100 * smooth_weight(distance, shoulder_radius) / 1024;
            if (distance < core_radius) {
                core = strength * 62 / 100 * smooth_weight(distance, core_radius) / 1024;
                if (saddle) core = core * 55 / 100;
            }
            lift = (shoulder + core) * taper / 100;
            if (lift <= 0) continue;
            if (lift >= context->mountain_uplift[index]) {
                lift += context->mountain_uplift[index] / 8;
            } else {
                lift = context->mountain_uplift[index] + lift / 8;
            }
            context->mountain_uplift[index] = (int16_t)clamp_int(lift, 0, 62);
            context->elevation[index] = (int16_t)clamp_int(
                context->base_elevation[index] + context->mountain_uplift[index], 0, 100);
        }
    }
}

static int find_seed(WorldGenContext *context, WorldGenRng *rng, int *out_x, int *out_y) {
    int attempt;
    for (attempt = 0; attempt < 700; attempt++) {
        int x = world_gen_rng_bounded(rng, context->width);
        int y = world_gen_rng_bounded(rng, context->height);
        int index = world_gen_context_index(context, x, y);
        if (context->land_mask[index] && context->ocean_distance[index] >= 4) {
            *out_x = x;
            *out_y = y;
            return 1;
        }
    }
    return 0;
}

static int endpoint_taper(int step, int length) {
    int ramp = length / 7;
    int remaining = length - 1 - step;
    int taper = 100;
    if (ramp < 5) ramp = 5;
    if (ramp > 8) ramp = 8;
    if (step < ramp) taper = 25 + step * 75 / ramp;
    if (remaining < ramp) {
        int end_taper = 25 + remaining * 75 / ramp;
        if (end_taper < taper) taper = end_taper;
    }
    return clamp_int(taper, 20, 100);
}

static int trace_range(WorldGenContext *context, WorldGenRng *rng, int start_x, int start_y,
                       int direction, int length, int base_radius, int base_strength,
                       int allow_branches) {
    static const int vector_x[16] = {1024, 946, 724, 392, 0, -392, -724, -946,
                                    -1024, -946, -724, -392, 0, 392, 724, 946};
    static const int vector_y[16] = {0, 392, 724, 946, 1024, 946, 724, 392,
                                    0, -392, -724, -946, -1024, -946, -724, -392};
    int px = start_x * 1024 + 512;
    int py = start_y * 1024 + 512;
    int turn_velocity = 0;
    int radius_delta = 0;
    int strength_delta = 0;
    int carved = 0;
    int pass_center = 18 + world_gen_rng_bounded(rng, 25);
    int step;

    for (step = 0; step < length; step++) {
        int x = px / 1024;
        int y = py / 1024;
        int radius;
        int core_radius;
        int strength;
        int saddle;
        int index;
        if (x < 2 || y < 2 || x >= context->width - 2 || y >= context->height - 2) break;
        index = world_gen_context_index(context, x, y);
        if (!context->land_mask[index]) break;
        if (step % 7 == 0) {
            turn_velocity = clamp_int(turn_velocity + world_gen_rng_signed(rng, 1), -1, 1);
            radius_delta = clamp_int(radius_delta + world_gen_rng_signed(rng, 1), -3, 4);
            strength_delta = clamp_int(strength_delta + world_gen_rng_signed(rng, 2), -8, 10);
        }
        if (step % 15 == 0 && turn_velocity != 0) direction = (direction + turn_velocity + 16) % 16;
        radius = clamp_int(base_radius + radius_delta, 5, 22);
        core_radius = clamp_int(radius / 3, 2, 7);
        strength = clamp_int(base_strength + strength_delta, 16, 55);
        saddle = abs(step - pass_center) <= 2;
        apply_profile(context, x, y, radius, core_radius, strength,
                      endpoint_taper(step, length), saddle);
        carved++;
        if (allow_branches && step > length / 5 && step < length * 4 / 5 &&
            step % 31 == 0) {
            int branch_roll = world_gen_rng_bounded(rng, 100);
            int branch_side = world_gen_rng_bounded(rng, 2);
            int branch_jitter = world_gen_rng_bounded(rng, length / 5 + 1);
            uint32_t branch_seed = world_gen_rng_next(rng);
            if (branch_roll < 18 + context->config.bias_mountain / 3) {
                WorldGenRng branch_rng;
                int branch_direction = (direction + (branch_side ? 3 : 13)) % 16;
                int branch_length = clamp_int(length / 5 + branch_jitter, 12, 55);
                world_gen_rng_init(&branch_rng, branch_seed);
                trace_range(context, &branch_rng, x, y, branch_direction, branch_length,
                            clamp_int(radius * 2 / 3, 4, 13), strength * 3 / 5, 0);
            }
        }
        if (allow_branches && step % 43 == 21 && world_gen_rng_bounded(rng, 100) < 18) {
            apply_profile(context, x, y, clamp_int(radius + 7, 9, 26),
                          clamp_int(core_radius + 3, 3, 9), strength * 3 / 4, 90, 0);
        }
        px += vector_x[direction];
        py += vector_y[direction];
        if (step == pass_center + 8) pass_center += 28 + world_gen_rng_bounded(rng, 24);
    }
    return carved;
}

int world_gen_apply_mountains(WorldGenContext *context) {
    WorldGenRng rng;
    int chains;
    int total_length = 0;
    int i;
    if (!context) return 0;
    world_gen_rng_init(&rng, context->phase_seed[WORLD_GEN_PHASE_MOUNTAIN]);
    chains = clamp_int(context->tile_count / 90000 +
                       context->config.bias_mountain / 18, 2, 14);
    last_chain_count = 0;
    last_average_length = 0;
    for (i = 0; i < chains; i++) {
        int x;
        int y;
        int direction;
        int length;
        int radius;
        int strength;
        int carved;
        if (!find_seed(context, &rng, &x, &y)) continue;
        direction = world_gen_rng_bounded(&rng, 16);
        length = clamp_int((context->width + context->height) / 10 +
                           world_gen_rng_bounded(&rng, (context->width + context->height) / 9 + 1),
                           35, 190);
        radius = clamp_int(5 + context->config.bias_mountain / 9 +
                           world_gen_rng_bounded(&rng, 5), 5, 20);
        strength = clamp_int(10 + context->config.relief * 28 / 100 +
                             world_gen_rng_bounded(&rng, 12), 12, 50);
        carved = trace_range(context, &rng, x, y, direction, length, radius, strength, 1);
        if (carved <= 0) continue;
        last_chain_count++;
        total_length += carved;
    }
    last_average_length = last_chain_count > 0 ? total_length / last_chain_count : 0;
    return 1;
}

int world_mountain_chain_count(void) {
    return last_chain_count;
}

int world_mountain_average_chain_length(void) {
    return last_average_length;
}
