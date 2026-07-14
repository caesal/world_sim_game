#ifndef WORLD_SIM_PLAGUE_ENGINE_H
#define WORLD_SIM_PLAGUE_ENGINE_H

#include <stdint.h>

typedef struct {
    int initialized;
    int phase;
    int absolute_month;
    int mortality_cursor;
    int any_deaths;
    uint64_t accumulated_us;
} PlagueUpdateState;

int plague_engine_update_month_step(PlagueUpdateState *state, int batch_size);

#endif
