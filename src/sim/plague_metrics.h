#ifndef WORLD_SIM_PLAGUE_METRICS_H
#define WORLD_SIM_PLAGUE_METRICS_H

#include "sim/plague_types.h"

extern PlagueMetricsSnapshot plague_metrics_state;

void plague_metrics_reset(void);
void plague_metrics_record_step(uint64_t elapsed_us, int active_city_count,
                                int due_pulse_count, int candidate_edge_count,
                                int committed_infection_count,
                                int deduplicated_request_count,
                                int spores_remaining, int episode_id);
void plague_metrics_record_month(uint64_t elapsed_us,
                                 const PlagueMetricsSnapshot *sample);
void plague_metrics_snapshot(PlagueMetricsSnapshot *out);

#endif
