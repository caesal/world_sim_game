#include "world/world_gen_diagnostics.h"

#include "core/world_types.h"
#include "world/world_gen_context.h"

static size_t active_peak_bytes(const WorldGenDiagnostics *diagnostics) {
    return diagnostics->context_bytes + diagnostics->hydrology_bytes +
           diagnostics->staged_path_bytes;
}

void world_gen_diagnostics_collect_counts(WorldGenDiagnostics *diagnostics,
                                          const WorldGenContext *context) {
    int i;
    if (!diagnostics || !context || context->tile_count <= 0) return;
    diagnostics->land_tiles = 0;
    diagnostics->ocean_tiles = 0;
    diagnostics->mountain_tiles = 0;
    diagnostics->river_tiles = 0;
    if (!context->land_mask) return;
    for (i = 0; i < context->tile_count; i++) {
        if (context->land_mask[i]) diagnostics->land_tiles++;
        else diagnostics->ocean_tiles++;
        if (context->geography &&
            (context->geography[i] == GEO_MOUNTAIN ||
             context->geography[i] == GEO_VOLCANO)) {
            diagnostics->mountain_tiles++;
        }
        if (context->river_flags &&
            (context->river_flags[i] & WORLD_GEN_RIVER_CHANNEL)) {
            diagnostics->river_tiles++;
        }
    }
}

void world_gen_diagnostics_finalize_failure(WorldGenDiagnostics *diagnostics,
                                            const WorldGenContext *context,
                                            WorldGenAttemptStage stage,
                                            uint64_t stage_ms,
                                            uint64_t total_ms) {
    size_t peak;
    if (!diagnostics || !context) return;
    if (stage == WORLDGEN_ATTEMPT_PREPARE_ELEVATION) {
        diagnostics->elevation_ms = stage_ms;
    } else if (stage == WORLDGEN_ATTEMPT_PREPARE_MOUNTAINS) {
        diagnostics->mountain_ms = stage_ms;
    } else if (stage == WORLDGEN_ATTEMPT_PREPARE_CLIMATE) {
        diagnostics->climate_ms = stage_ms;
    } else if (stage == WORLDGEN_ATTEMPT_PREPARE_HYDROLOGY) {
        diagnostics->hydrology_ms = stage_ms;
    } else if (stage == WORLDGEN_ATTEMPT_PREPARE_CLASSIFY) {
        diagnostics->classification_ms = stage_ms;
    }
    diagnostics->total_ms = total_ms;
    world_gen_diagnostics_collect_counts(diagnostics, context);
    peak = active_peak_bytes(diagnostics);
    if (diagnostics->peak_bytes < peak) diagnostics->peak_bytes = peak;
    worldgen_attempt_note_generated_counts(diagnostics->land_tiles,
                                            diagnostics->ocean_tiles,
                                            diagnostics->river_tiles);
    worldgen_attempt_note_memory((uint64_t)diagnostics->peak_bytes);
    worldgen_attempt_note_elapsed(total_ms);
}
