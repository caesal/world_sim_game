#include "world/river_hydroclimate.h"

#include <stdlib.h>
#include <string.h>

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int max_int(int left, int right) {
    return left > right ? left : right;
}

static int source_radius(const RiverGenerationState *state, int index) {
    uint16_t flags = state->cell_flags[index];
    int precipitation = clamp_int(state->input.precipitation[index], 0, 100);
    int radius = 0;

    if (flags & RIVER_CELL_DELTA) radius = 5;
    else if (flags & RIVER_CELL_LAKE) radius = 3;
    else if (flags & RIVER_CELL_CHANNEL) radius = 1 + state->input.width_field[index] / 3;
    if (flags & RIVER_CELL_CONFLUENCE) radius++;
    radius = clamp_int(radius, 1, 6);
    if (precipitation < 30 && !(flags & RIVER_CELL_DELTA)) radius = clamp_int(radius, 1, 2);
    return radius;
}

static int source_strength(const RiverGenerationState *state, int index) {
    uint16_t flags = state->cell_flags[index];
    int strength = 3;

    if (flags & RIVER_CELL_CHANNEL) strength += state->input.width_field[index];
    if (flags & RIVER_CELL_LAKE) strength += 5;
    if (flags & RIVER_CELL_CONFLUENCE) strength += 5;
    if (flags & RIVER_CELL_DELTA) strength += 9;
    return clamp_int(strength, 3, 24);
}

static void apply_source(RiverGenerationState *state, int source) {
    int sx = source % state->input.width;
    int sy = source / state->input.width;
    int radius = source_radius(state, source);
    int strength = source_strength(state, source);
    int dx;
    int dy;

    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            int x = sx + dx;
            int y = sy + dy;
            int distance = max_int(abs(dx), abs(dy));
            int index;
            int moisture_delta;
            int temperature_delta;
            int fertility;
            int local_slope;
            if (x < 0 || x >= state->input.width || y < 0 || y >= state->input.height) continue;
            if (distance > radius) continue;
            index = y * state->input.width + x;
            if (!state->input.land_mask[index]) continue;
            state->diagnostics.bounded_influence_visits++;
            moisture_delta = strength * (radius + 1 - distance) / (radius + 1);
            if (moisture_delta > state->moisture_influence[index]) {
                state->moisture_influence[index] = (uint8_t)moisture_delta;
            }
            temperature_delta = clamp_int(moisture_delta / 4, 1, 4);
            if (state->input.temperature && state->input.temperature[index] > 55) {
                temperature_delta = -temperature_delta;
            }
            if (abs(temperature_delta) > abs(state->temperature_influence[index])) {
                state->temperature_influence[index] = (int8_t)temperature_delta;
            }
            local_slope = state->input.slope ? abs(state->input.slope[index]) : 0;
            fertility = 12 + moisture_delta * 4 - local_slope / 2;
            if (state->cell_flags[source] & RIVER_CELL_CONFLUENCE) fertility += 10;
            if (state->cell_flags[source] & RIVER_CELL_DELTA) fertility += 18;
            if (state->input.width_field[source] >= 4 && local_slope <= 6) fertility += 12;
            fertility = clamp_int(fertility, 0, 100);
            if (fertility > state->input.soil_fertility[index]) {
                state->input.soil_fertility[index] = (uint8_t)fertility;
            }
        }
    }
}

static void initialize_fertility(RiverGenerationState *state) {
    int i;

    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        int precipitation = clamp_int(state->input.precipitation[index], 0, 100);
        int slope = state->input.slope ? abs(state->input.slope[index]) : 0;
        int base = clamp_int(8 + precipitation / 3 - slope / 3, 0, 60);
        state->input.soil_fertility[index] = (uint8_t)base;
    }
}

int river_hydroclimate_apply(RiverGenerationState *state) {
    int i;

    if (!state) return 0;
    memset(state->moisture_influence, 0,
           (size_t)state->input.tile_count * sizeof(*state->moisture_influence));
    memset(state->temperature_influence, 0,
           (size_t)state->input.tile_count * sizeof(*state->temperature_influence));
    initialize_fertility(state);
    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        if (state->cell_flags[index] & (RIVER_CELL_CHANNEL | RIVER_CELL_LAKE)) {
            apply_source(state, index);
        }
    }
    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        if (state->input.moisture) {
            state->input.moisture[index] = (int16_t)clamp_int(
                state->input.moisture[index] + state->moisture_influence[index], 0, 100);
        }
        if (state->input.temperature) {
            state->input.temperature[index] = (int16_t)clamp_int(
                state->input.temperature[index] + state->temperature_influence[index], 0, 100);
        }
    }
    return 1;
}
