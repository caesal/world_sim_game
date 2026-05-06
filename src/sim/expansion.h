#ifndef WORLD_SIM_EXPANSION_H
#define WORLD_SIM_EXPANSION_H

#include <stddef.h>

typedef struct {
    int active;
    int civ_id;
    int resource_score;
    int resource_pressure;
    int attempts_remaining;
    int stage;
    int sample_index;
    int target_found;
    int best_x;
    int best_y;
    int best_score;
    int target_score;
} ExpansionWorkState;

void expansion_update_civilization(int civ_id, int resource_score, char *log, size_t log_size);
void expansion_work_begin(ExpansionWorkState *work, int civ_id, int resource_score);
int expansion_work_step(ExpansionWorkState *work, char *log, size_t log_size);
int expansion_work_done(const ExpansionWorkState *work);
int expansion_need_for_civ(int civ_id, int resource_score);
int expansion_threshold_for_civ(int civ_id);

#endif
