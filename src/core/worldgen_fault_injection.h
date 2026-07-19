#ifndef WORLD_SIM_WORLDGEN_FAULT_INJECTION_H
#define WORLD_SIM_WORLDGEN_FAULT_INJECTION_H

typedef enum {
    WORLDGEN_FAULT_NONE = 0,
    WORLDGEN_FAULT_PREPARE_ALLOCATION,
    WORLDGEN_FAULT_RIVER_WORKSPACE_ALLOCATION,
    WORLDGEN_FAULT_RIVER_DISTRIBUTARY_ALLOCATION,
    WORLDGEN_FAULT_RIVER_SEGMENT_ALLOCATION,
    WORLDGEN_FAULT_RIVER_PATH_ALLOCATION,
    WORLDGEN_FAULT_RIVER_PATH_COPY,
    WORLDGEN_FAULT_SNAPSHOT_PUBLISH,
    WORLDGEN_FAULT_SNAPSHOT_RIVER_COPY,
    WORLDGEN_FAULT_PREWARM_ALLOCATION,
    WORLDGEN_FAULT_COUNT
} WorldGenFaultPoint;

void worldgen_fault_injection_arm(WorldGenFaultPoint point,
                                  int first_call, int failure_count);
void worldgen_fault_injection_clear(void);
int worldgen_fault_injection_should_fail(WorldGenFaultPoint point);
int worldgen_fault_injection_was_triggered(WorldGenFaultPoint point);
int worldgen_fault_injection_call_count(WorldGenFaultPoint point);

#endif
