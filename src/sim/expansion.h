#ifndef WORLD_SIM_EXPANSION_H
#define WORLD_SIM_EXPANSION_H

#include <stddef.h>

#include "sim/maritime.h"

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

typedef struct {
    int land_adjacent_unowned_regions;
    int land_nearby_unowned_regions;
    int shallow_sea_reachable_regions;
    int maritime_reachable_regions;
    int deep_sea_reachable_regions;
    int port_candidate_regions;
    int adjacent_unowned_regions;
    int nearby_unowned_regions;
    int port_unowned_regions;
    int global_unowned_regions;
    int global_unowned_percent;
    int population_pressure;
    int resource_pressure;
    int expansion_need;
    int expansion_threshold;
    int expansion_desire;
    int tech_expansion_percent;
    int claim_cooldown_months;
    int months_until_next_claim;
    int claim_budget;
    MaritimeExpansionDiagnostics maritime;
} ExpansionAIDiagnostics;

void expansion_update_civilization(int civ_id, int resource_score, char *log, size_t log_size);
void expansion_reset(void);
void expansion_work_begin(ExpansionWorkState *work, int civ_id, int resource_score);
int expansion_work_step(ExpansionWorkState *work, char *log, size_t log_size);
int expansion_work_done(const ExpansionWorkState *work);
int expansion_need_for_civ(int civ_id, int resource_score);
int expansion_threshold_for_civ(int civ_id);
int expansion_resource_score_for_civ(int civ_id);
int expansion_civ_months_until_claim(int civ_id);
ExpansionAIDiagnostics expansion_ai_diagnostics(int civ_id, int resource_score);
const char *expansion_last_reason(int civ_id);

#endif
