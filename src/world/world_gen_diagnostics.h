#ifndef WORLD_SIM_WORLD_GEN_DIAGNOSTICS_H
#define WORLD_SIM_WORLD_GEN_DIAGNOSTICS_H

#include <stdint.h>

#include "core/worldgen_attempt.h"
#include "world/world_gen.h"

typedef struct WorldGenContext WorldGenContext;

void world_gen_diagnostics_collect_counts(WorldGenDiagnostics *diagnostics,
                                          const WorldGenContext *context);
void world_gen_diagnostics_finalize_failure(WorldGenDiagnostics *diagnostics,
                                            const WorldGenContext *context,
                                            WorldGenAttemptStage stage,
                                            uint64_t stage_ms,
                                            uint64_t total_ms);

#endif
