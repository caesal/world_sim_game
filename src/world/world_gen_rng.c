#include "world_gen_rng.h"

static uint64_t mix64(uint64_t value) {
    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31;
    return value;
}

uint32_t world_gen_derive_seed(uint32_t master_seed, uint32_t stream_id) {
    uint64_t mixed = mix64(((uint64_t)master_seed << 32) |
                           (uint64_t)(stream_id * UINT32_C(0x9e3779b9)));
    return (uint32_t)(mixed ^ (mixed >> 32));
}

void world_gen_rng_init(WorldGenRng *rng, uint32_t seed) {
    if (!rng) return;
    rng->state = mix64((uint64_t)seed + UINT64_C(0x9e3779b97f4a7c15));
    if (rng->state == 0) rng->state = UINT64_C(0x2545f4914f6cdd1d);
}

uint32_t world_gen_rng_next(WorldGenRng *rng) {
    uint64_t value;
    if (!rng) return 0;
    value = rng->state;
    value ^= value >> 12;
    value ^= value << 25;
    value ^= value >> 27;
    rng->state = value;
    return (uint32_t)((value * UINT64_C(2685821657736338717)) >> 32);
}

int world_gen_rng_bounded(WorldGenRng *rng, int upper_bound) {
    uint32_t limit;
    uint32_t value;
    if (upper_bound <= 0) return 0;
    limit = UINT32_MAX - (UINT32_MAX % (uint32_t)upper_bound);
    do {
        value = world_gen_rng_next(rng);
    } while (value >= limit);
    return (int)(value % (uint32_t)upper_bound);
}

int world_gen_rng_signed(WorldGenRng *rng, int magnitude) {
    if (magnitude <= 0) return 0;
    return world_gen_rng_bounded(rng, magnitude * 2 + 1) - magnitude;
}

uint32_t world_gen_hash_u32(uint32_t seed, uint32_t value) {
    uint64_t mixed = mix64(((uint64_t)seed << 32) | value);
    return (uint32_t)(mixed ^ (mixed >> 32));
}
