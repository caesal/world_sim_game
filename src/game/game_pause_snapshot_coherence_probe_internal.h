#ifndef WORLD_SIM_GAME_PAUSE_SNAPSHOT_COHERENCE_PROBE_INTERNAL_H
#define WORLD_SIM_GAME_PAUSE_SNAPSHOT_COHERENCE_PROBE_INTERNAL_H

#include "sim/decision_snapshot_cache.h"
#include "sim/plague_types.h"

#include <stdio.h>

typedef struct {
    int episode_id;
    int start_month;
    int end_month;
    int duration_months;
} PauseProbeExpectedEpisode;

typedef struct {
    int year;
    int month;
    int model_history_count;
    int model_active;
    int model_episode_id;
    int model_episode_start;
    PlagueEpisodeHistory model_history;
    int plague_key;
    int cache_dirty;
    int cache_valid;
    int cache_history_count;
    int cache_active;
    int cache_episode_id;
    PlagueEpisodeHistory cache_history;
    int front_valid;
    int front_year;
    int front_month;
    int front_history_count;
    int front_active;
    int front_episode_id;
    int front_plague_revision;
    unsigned int front_revision;
    PlagueEpisodeHistory front_history;
    int scheduler_pending;
    int scheduler_active;
    int scheduler_queued;
    int worker_pending;
    int pause_in_progress;
    int pause_settled;
    DecisionSnapshotCacheDiagnostics decision;
} PauseProbeObservation;

typedef struct {
    FILE *summary;
    int case_count;
    int pass_count;
} PauseProbeReport;

int pause_probe_setup_fixture(void);
int pause_probe_cleanup(void);
int pause_probe_seed_episode(int episode_id, int duration_months,
                             int completes_this_month,
                             PauseProbeExpectedEpisode *expected);
int pause_probe_queue_months(int count);
int pause_probe_run_until_history(int history_count, int max_steps);
int pause_probe_run_until_plague_revision(int initial_revision, int require_active,
                                          int max_steps);
int pause_probe_wait_settled(int timeout_ms);
int pause_probe_wait_calendar_front(int target_year, int target_month, int timeout_ms);
void pause_probe_observe(PauseProbeObservation *out);
int pause_probe_history_coherent(const PauseProbeObservation *observation,
                                 const PauseProbeExpectedEpisode *expected);
int pause_probe_active_coherent(const PauseProbeObservation *observation,
                                const PauseProbeExpectedEpisode *expected);
int pause_probe_report(PauseProbeReport *report, const char *name, int ok,
                       const char *format, ...);
int pause_probe_run_cases(PauseProbeReport *report);

#endif
