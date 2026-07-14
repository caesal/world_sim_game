#ifndef WORLD_SIM_PLAGUE_SPREAD_H
#define WORLD_SIM_PLAGUE_SPREAD_H

#include "sim/plague_types.h"

typedef struct {
    int source_count;
    int due_pulses;
    int candidate_edges;
    int candidate_edges_by_route[PLAGUE_ROUTE_COUNT];
    int pending_requests;
    int deduplicated_requests;
    int committed_infections;
    int committed_persistence;
} PlagueSpreadStats;

void plague_spread_reset(void);
void plague_spread_begin_month(int absolute_month);
int plague_spread_gather_step(int source_budget);
int plague_spread_prepare_commits(void);
int plague_spread_commit_step(int request_budget);
int plague_spread_gather_done(void);
int plague_spread_commit_done(void);
void plague_spread_stats(PlagueSpreadStats *out);

#endif
