#include "sim/plague_metrics.h"

#include <string.h>

PlagueMetricsSnapshot plague_metrics_state;

void plague_metrics_reset(void) {
    memset(&plague_metrics_state, 0, sizeof(plague_metrics_state));
}

void plague_metrics_record_step(uint64_t elapsed_us, int active_city_count,
                                int due_pulse_count, int candidate_edge_count,
                                int committed_infection_count,
                                int deduplicated_request_count,
                                int spores_remaining, int episode_id) {
    plague_metrics_state.step_last_us = elapsed_us;
    plague_metrics_state.step_total_us += elapsed_us;
    plague_metrics_state.step_samples++;
    if (elapsed_us > plague_metrics_state.step_peak_us) {
        plague_metrics_state.step_peak_us = elapsed_us;
    }
    plague_metrics_state.active_city_count = active_city_count;
    plague_metrics_state.due_pulse_count = due_pulse_count;
    plague_metrics_state.candidate_edge_count = candidate_edge_count;
    plague_metrics_state.committed_infection_count = committed_infection_count;
    plague_metrics_state.deduplicated_request_count = deduplicated_request_count;
    plague_metrics_state.spores_remaining = spores_remaining;
    plague_metrics_state.episode_id = episode_id;
}

void plague_metrics_record_month(uint64_t elapsed_us,
                                 const PlagueMetricsSnapshot *sample) {
    uint64_t previous_total = plague_metrics_state.step_total_us;
    uint64_t previous_peak = plague_metrics_state.step_peak_us;
    uint64_t previous_samples = plague_metrics_state.step_samples;
    if (sample) plague_metrics_state = *sample;
    plague_metrics_state.step_last_us = elapsed_us;
    plague_metrics_state.step_total_us = previous_total + elapsed_us;
    plague_metrics_state.step_samples = previous_samples + 1;
    plague_metrics_state.step_peak_us =
        elapsed_us > previous_peak ? elapsed_us : previous_peak;
}

void plague_metrics_snapshot(PlagueMetricsSnapshot *out) {
    if (out) *out = plague_metrics_state;
}
