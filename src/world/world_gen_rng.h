#ifndef WORLD_SIM_WORLD_GEN_RNG_H
#define WORLD_SIM_WORLD_GEN_RNG_H

#include <stdint.h>

typedef struct {
    uint64_t state;
} WorldGenRng;

uint32_t world_gen_derive_seed(uint32_t master_seed, uint32_t stream_id);
void world_gen_rng_init(WorldGenRng *rng, uint32_t seed);
uint32_t world_gen_rng_next(WorldGenRng *rng);
int world_gen_rng_bounded(WorldGenRng *rng, int upper_bound);
int world_gen_rng_signed(WorldGenRng *rng, int magnitude);
uint32_t world_gen_hash_u32(uint32_t seed, uint32_t value);

#endif
