#ifndef WORLD_SIM_GAME_WORLDGEN_CLIMATE_PROBE_H
#define WORLD_SIM_GAME_WORLDGEN_CLIMATE_PROBE_H

#include <stdint.h>
#include <stdio.h>

#include "world/world_gen_context.h"

int game_worldgen_climate_probe_check_context(FILE *file, const char *label,
                                               const WorldGenContext *context,
                                               int require_full_domain);
uint64_t game_worldgen_climate_probe_hash(const WorldGenContext *context);
int game_worldgen_climate_probe_compare(FILE *file, const char *label,
                                        const WorldGenContext *first,
                                        const WorldGenContext *second);
int game_worldgen_climate_probe_transport_contract(FILE *file);

#endif
