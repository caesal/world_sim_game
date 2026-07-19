#include "world_gen_context.h"

#include "core/worldgen_attempt.h"
#include "core/worldgen_fault_injection.h"
#include "world/world_gen_rng.h"

#include <stdlib.h>
#include <string.h>

static int allocate_field(void **field, size_t count, size_t item_size,
                          WorldGenContext *context, int field_index) {
    if (worldgen_fault_injection_should_fail(WORLDGEN_FAULT_PREPARE_ALLOCATION)) {
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_PREPARE_ALLOCATION_INJECTED);
        worldgen_attempt_note_context_allocation(
            context->allocated_bytes, field_index,
            sizeof(*context) + context->allocated_bytes);
        return 0;
    }
    *field = calloc(count, item_size);
    if (!*field) {
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_PREPARE_FIELD_ALLOCATION);
        worldgen_attempt_note_context_allocation(
            context->allocated_bytes, field_index,
            sizeof(*context) + context->allocated_bytes);
        return 0;
    }
    context->allocated_bytes += count * item_size;
    worldgen_attempt_note_context_allocation(
        context->allocated_bytes, 0, sizeof(*context) + context->allocated_bytes);
    return 1;
}

#define ALLOCATE(name) \
    allocate_field((void **)&context->name, count, sizeof(*context->name), \
                   context, ++allocation_field)

int world_gen_context_create(WorldGenContext *context, const WorldGenConfig *config,
                             int width, int height, uint32_t master_seed) {
    size_t count;
    int allocation_field = 0;
    int stage;

    if (!context || width <= 0 || height <= 0 ||
        width > MAX_MAP_W || height > MAX_MAP_H) {
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_PREPARED_VALIDATION);
        return 0;
    }
    memset(context, 0, sizeof(*context));
    context->width = width;
    context->height = height;
    context->tile_count = width * height;
    context->sea_level = 50;
    context->config = config ? *config : DEFAULT_WORLD_GEN_CONFIG;
    context->config.seed = master_seed;
    context->config.random_seed = 0;
    context->master_seed = master_seed;
    for (stage = 0; stage < WORLD_GEN_PHASE_COUNT; stage++) {
        context->phase_seed[stage] = world_gen_derive_seed(master_seed, (uint32_t)stage + 1u);
    }
    count = (size_t)context->tile_count;

    if (!ALLOCATE(base_elevation) || !ALLOCATE(elevation) ||
        !ALLOCATE(relative_altitude) || !ALLOCATE(slope) || !ALLOCATE(curvature) ||
        !ALLOCATE(mountain_uplift) || !ALLOCATE(ocean_distance) ||
        !ALLOCATE(moisture) || !ALLOCATE(temperature) || !ALLOCATE(precipitation) ||
        !ALLOCATE(land_mask) || !ALLOCATE(coastal_lowland_hint) ||
        !ALLOCATE(wind_direction16) || !ALLOCATE(wind_speed) ||
        !ALLOCATE(geography) || !ALLOCATE(climate) || !ALLOCATE(ecology) ||
        !ALLOCATE(resource) || !ALLOCATE(resource_variation) || !ALLOCATE(river_order) ||
        !ALLOCATE(river_flags) || !ALLOCATE(soil_fertility) || !ALLOCATE(river_width) ||
        !ALLOCATE(drainage_receiver) || !ALLOCATE(drainage_basin) ||
        !ALLOCATE(topological_order) || !ALLOCATE(runoff) || !ALLOCATE(river_flow) ||
        !ALLOCATE(upstream_count) || !ALLOCATE(scratch_a) || !ALLOCATE(scratch_b)) {
        world_gen_context_destroy(context);
        return 0;
    }
    return 1;
}

#define RELEASE(name) do { free(context->name); context->name = NULL; } while (0)

void world_gen_context_destroy(WorldGenContext *context) {
    if (!context) return;
    RELEASE(staged_river_paths);
    RELEASE(base_elevation);
    RELEASE(elevation);
    RELEASE(relative_altitude);
    RELEASE(slope);
    RELEASE(curvature);
    RELEASE(mountain_uplift);
    RELEASE(ocean_distance);
    RELEASE(moisture);
    RELEASE(temperature);
    RELEASE(precipitation);
    RELEASE(land_mask);
    RELEASE(coastal_lowland_hint);
    RELEASE(wind_direction16);
    RELEASE(wind_speed);
    RELEASE(geography);
    RELEASE(climate);
    RELEASE(ecology);
    RELEASE(resource);
    RELEASE(resource_variation);
    RELEASE(river_order);
    RELEASE(river_flags);
    RELEASE(soil_fertility);
    RELEASE(river_width);
    RELEASE(drainage_receiver);
    RELEASE(drainage_basin);
    RELEASE(topological_order);
    RELEASE(runoff);
    RELEASE(river_flow);
    RELEASE(upstream_count);
    RELEASE(scratch_a);
    RELEASE(scratch_b);
    memset(context, 0, sizeof(*context));
}

int world_gen_context_index(const WorldGenContext *context, int x, int y) {
    return y * context->width + x;
}

int world_gen_context_in_bounds(const WorldGenContext *context, int x, int y) {
    return context && x >= 0 && y >= 0 && x < context->width && y < context->height;
}

#undef ALLOCATE
#undef RELEASE
