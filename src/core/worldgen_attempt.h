#ifndef WORLD_SIM_WORLDGEN_ATTEMPT_H
#define WORLD_SIM_WORLDGEN_ATTEMPT_H

#include <stdint.h>

typedef enum {
    WORLDGEN_ATTEMPT_IDLE = 0,
    WORLDGEN_ATTEMPT_PREPARE_CONTEXT,
    WORLDGEN_ATTEMPT_PREPARE_ELEVATION,
    WORLDGEN_ATTEMPT_PREPARE_MOUNTAINS,
    WORLDGEN_ATTEMPT_PREPARE_CLIMATE,
    WORLDGEN_ATTEMPT_PREPARE_HYDROLOGY,
    WORLDGEN_ATTEMPT_PREPARE_CLASSIFY,
    WORLDGEN_ATTEMPT_PREPARE_RIVER_PATHS,
    WORLDGEN_ATTEMPT_VALIDATE,
    WORLDGEN_ATTEMPT_COMMIT,
    WORLDGEN_ATTEMPT_REGIONS,
    WORLDGEN_ATTEMPT_PORTS,
    WORLDGEN_ATTEMPT_CIVILIZATIONS,
    WORLDGEN_ATTEMPT_ROUTE_POTENTIAL,
    WORLDGEN_ATTEMPT_FINALIZE,
    WORLDGEN_ATTEMPT_SNAPSHOT_PUBLISH,
    WORLDGEN_ATTEMPT_STATIC_PREWARM,
    WORLDGEN_ATTEMPT_COMPLETE
} WorldGenAttemptStage;

typedef enum {
    WORLDGEN_FAILURE_NONE = 0,
    WORLDGEN_FAILURE_PREPARE_CONTEXT_ALLOCATION,
    WORLDGEN_FAILURE_PREPARE_FIELD_ALLOCATION,
    WORLDGEN_FAILURE_PREPARE_ALLOCATION_INJECTED,
    WORLDGEN_FAILURE_ELEVATION,
    WORLDGEN_FAILURE_MOUNTAINS,
    WORLDGEN_FAILURE_CLIMATE,
    WORLDGEN_FAILURE_HYDROLOGY,
    WORLDGEN_FAILURE_RIVER_WORKSPACE_ALLOCATION,
    WORLDGEN_FAILURE_RIVER_DISTRIBUTARY_ALLOCATION,
    WORLDGEN_FAILURE_RIVER_SEGMENT_ALLOCATION,
    WORLDGEN_FAILURE_CLASSIFICATION,
    WORLDGEN_FAILURE_RIVER_NETWORK,
    WORLDGEN_FAILURE_RIVER_PATH_COUNT_INVALID,
    WORLDGEN_FAILURE_RIVER_PATH_ALLOCATION,
    WORLDGEN_FAILURE_RIVER_PATH_COPY,
    WORLDGEN_FAILURE_RIVER_PATH_VALIDATION,
    WORLDGEN_FAILURE_PREPARED_VALIDATION,
    WORLDGEN_FAILURE_COMMIT_PHYSICAL_STATE,
    WORLDGEN_FAILURE_SNAPSHOT_PUBLISH,
    WORLDGEN_FAILURE_PREWARM_ALLOCATION_INJECTED,
    WORLDGEN_FAILURE_PREWARM_BUILD,
    WORLDGEN_FAILURE_LAND_MASK_INVALID_TARGET,
    WORLDGEN_FAILURE_LAND_MASK_FRONTIER_STALLED,
    WORLDGEN_FAILURE_LAND_MASK_TARGET_DRIFT,
    WORLDGEN_FAILURE_LAND_MASK_SEMANTIC_ARTIFACT
} WorldGenFailureReason;

typedef struct {
    uint64_t attempt_id;
    WorldGenAttemptStage stage;
    WorldGenAttemptStage last_failure_stage;
    WorldGenFailureReason last_failure_reason;
    int active;
    int success;
    int world_committed;
    int snapshot_published;
    int snapshot_attempts;
    int deferred_snapshot_pending;
    int deferred_snapshot_attempts;
    int deferred_snapshot_succeeded;
    int prewarm_attempted;
    int prewarm_succeeded;
    int prewarm_attempts;
    int lazy_fallback_required;
    int target_width;
    int target_height;
    int target_land_tiles;
    int actual_land_tiles;
    int target_ocean_tiles;
    int actual_ocean_tiles;
    int river_channel_tiles;
    int river_paths_required;
    int river_paths_copied;
    int river_path_capacity;
    uint64_t context_allocated_bytes;
    int context_allocation_failure_field;
    uint64_t staged_allocation_bytes;
    uint64_t peak_allocation_bytes;
    uint64_t elapsed_ms;
    int previous_world_generated;
    int previous_physical_revision;
    uint64_t previous_physical_hash;
} WorldGenAttemptDiagnostics;

uint64_t worldgen_attempt_begin(void);
void worldgen_attempt_set_stage(WorldGenAttemptStage stage);
void worldgen_attempt_record_failure(WorldGenFailureReason reason);
void worldgen_attempt_note_commit(void);
void worldgen_attempt_note_snapshot(int succeeded, int attempts);
void worldgen_attempt_note_deferred_snapshot(uint64_t attempt_id, int succeeded);
void worldgen_attempt_note_prewarm(int succeeded, int attempts);
void worldgen_attempt_note_deferred_prewarm(uint64_t attempt_id, int succeeded);
void worldgen_attempt_note_target(int width, int height, int land_tiles,
                                  int ocean_tiles);
void worldgen_attempt_note_generated_counts(int land_tiles, int ocean_tiles,
                                            int river_channel_tiles);
void worldgen_attempt_note_river_paths(int required, int copied, int capacity,
                                       uint64_t staged_bytes);
void worldgen_attempt_note_context_allocation(uint64_t allocated_bytes,
                                              int failure_field,
                                              uint64_t peak_bytes);
void worldgen_attempt_note_memory(uint64_t peak_bytes);
void worldgen_attempt_note_elapsed(uint64_t elapsed_ms);
void worldgen_attempt_note_previous_world(int generated, int physical_revision,
                                          uint64_t physical_hash);
void worldgen_attempt_finish(int succeeded);
int worldgen_attempt_active(void);
void worldgen_attempt_get(WorldGenAttemptDiagnostics *out);
const char *worldgen_attempt_stage_name(WorldGenAttemptStage stage);
const char *worldgen_failure_reason_name(WorldGenFailureReason reason);
const char *worldgen_failure_reason_text_en(WorldGenFailureReason reason);
const char *worldgen_failure_reason_text_zh(WorldGenFailureReason reason);

#endif
