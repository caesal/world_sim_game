#ifndef WORLD_SIM_WORLD_GEN_ARIDITY_PROJECTION_H
#define WORLD_SIM_WORLD_GEN_ARIDITY_PROJECTION_H

#include <stdint.h>

#include "core/world_types.h"
#include "world/world_gen_context.h"

enum {
    WORLD_GEN_ARIDITY_PROJECTION_BIN_COUNT = 101,
    WORLD_GEN_ARIDITY_TRANSITION_NON_REFRESH = 0,
    WORLD_GEN_ARIDITY_TRANSITION_REFRESH,
    WORLD_GEN_ARIDITY_TRANSITION_KIND_COUNT
};

typedef struct {
    uint64_t climate_input_hash;
    int width;
    int height;
    uint32_t master_seed;
    int land_count;
    int eligible_count;
    int histogram[WORLD_GEN_ARIDITY_PROJECTION_BIN_COUNT];
    int prefix[WORLD_GEN_ARIDITY_PROJECTION_BIN_COUNT];
    int pre_climate_count[CLIMATE_COUNT];
    int post_climate_count[CLIMATE_COUNT];
    int transition_count[WORLD_GEN_ARIDITY_TRANSITION_KIND_COUNT]
        [CLIMATE_COUNT][CLIMATE_COUNT];
    int refresh_eligible_count;
    int refresh_changed_count;
    int non_refresh_changed_count;
    int transition_accounted_count;
    int accounting_unexplained_count;
    int accounting_ok;
    int complete;
} WorldGenAridityProjectionResult;

int world_gen_aridity_projection_validation_enable(void);
void world_gen_aridity_projection_validation_reset(void);
int world_gen_aridity_projection_validation_active(void);
int world_gen_aridity_projection_validation_matches(void);
int world_gen_aridity_projection_capture_begin_context(
    WorldGenContext *context);
int world_gen_aridity_projection_capture_pre(
    const WorldGenContext *context, int index, int eligible, Climate climate);
int world_gen_aridity_projection_capture_post(
    const WorldGenContext *context, int index, int refresh_eligible,
    Climate pre_climate, Climate post_climate);
int world_gen_aridity_projection_get_result(
    WorldGenAridityProjectionResult *result);

#endif
