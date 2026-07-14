#ifndef WORLD_SIM_PLAGUE_DIAGNOSTICS_H
#define WORLD_SIM_PLAGUE_DIAGNOSTICS_H

#include "core/constants.h"
#include "sim/plague_types.h"

#include <stdint.h>

typedef struct {
    int active;
    int episode_id;
    int size;
    int severity;
    int start_month;
    int age_months;
    int spores_initial;
    int spores_remaining;
    int active_cities;
    int infected_cities;
    int maximum_generation;
    int affected_countries;
    int64_t total_deaths;
    char name_en[NAME_LEN];
    char name_zh[NAME_LEN];
} PlagueActiveDiagnostics;

typedef struct {
    int episode_id;
    int size;
    int severity;
    int start_month;
    int end_month;
    int duration_months;
    int spores_initial;
    int spores_used;
    int infected_cities;
    int affected_countries;
    int64_t total_deaths;
    char name_en[NAME_LEN];
    char name_zh[NAME_LEN];
} PlagueHistoryDiagnostics;

typedef struct {
    PlagueActiveDiagnostics active;
    int history_count;
    PlagueHistoryDiagnostics history[PLAGUE_RECENT_HISTORY_CAP];
} PlagueDiagnosticsSnapshot;

extern PlagueDiagnosticsSnapshot plague_diagnostics_state;

void plague_diagnostics_reset(void);
void plague_diagnostics_refresh(int absolute_month);

#endif
