#include "core/profiler.h"

#include <stdio.h>
#include <string.h>

#define PROFILER_FRAME_HISTORY 60

static int frame_ms_history[PROFILER_FRAME_HISTORY];
static int sim_ms_history[PROFILER_FRAME_HISTORY];
static int render_ms_history[PROFILER_FRAME_HISTORY];
static int history_index;
static int history_count;
static RuntimeProfilerSnapshot snapshot_state;

static int history_average(const int *values) {
    int i;
    int total = 0;
    if (history_count <= 0) return 0;
    for (i = 0; i < history_count; i++) total += values[i];
    return total / history_count;
}

static int history_peak(const int *values) {
    int i;
    int peak = 0;
    for (i = 0; i < history_count; i++) {
        if (values[i] > peak) peak = values[i];
    }
    return peak;
}

void profiler_reset(void) {
    memset(frame_ms_history, 0, sizeof(frame_ms_history));
    memset(sim_ms_history, 0, sizeof(sim_ms_history));
    memset(render_ms_history, 0, sizeof(render_ms_history));
    memset(&snapshot_state, 0, sizeof(snapshot_state));
    history_index = 0;
    history_count = 0;
}

void profiler_begin_month(void) {
    snapshot_state.slowest_phase[0] = '\0';
    snapshot_state.slowest_phase_ms = 0;
    snapshot_state.resource_ms = 0;
    snapshot_state.population_ms = 0;
    snapshot_state.expansion_ms = 0;
    snapshot_state.claim_ms = 0;
    snapshot_state.diplomacy_ms = 0;
    snapshot_state.war_ms = 0;
    snapshot_state.plague_ms = 0;
    snapshot_state.claimed_regions_this_month = 0;
    snapshot_state.scanned_regions_this_month = 0;
    snapshot_state.scanned_tiles_this_month = 0;
    snapshot_state.expansion_civs_checked = 0;
    snapshot_state.expansion_claims_attempted = 0;
    snapshot_state.expansion_claims_succeeded = 0;
    snapshot_state.expansion_target_search_ms = 0;
    snapshot_state.claim_tiles_touched = 0;
    snapshot_state.maritime_path_searches = 0;
    snapshot_state.maritime_bfs_nodes = 0;
}

void profiler_record_frame(int frame_ms, int sim_budget_ms, int sim_used_ms,
                           int actual_ms_per_month, int pending_months, int overloaded) {
    frame_ms_history[history_index] = frame_ms;
    sim_ms_history[history_index] = sim_used_ms;
    history_index = (history_index + 1) % PROFILER_FRAME_HISTORY;
    if (history_count < PROFILER_FRAME_HISTORY) history_count++;
    snapshot_state.frame_avg_ms = history_average(frame_ms_history);
    snapshot_state.frame_peak_ms = history_peak(frame_ms_history);
    snapshot_state.sim_avg_ms = history_average(sim_ms_history);
    snapshot_state.sim_peak_ms = history_peak(sim_ms_history);
    snapshot_state.sim_budget_ms = sim_budget_ms;
    snapshot_state.sim_used_ms = sim_used_ms;
    snapshot_state.actual_ms_per_month = actual_ms_per_month;
    snapshot_state.pending_months = pending_months;
    snapshot_state.overloaded = overloaded;
}

void profiler_set_current_job(const char *job_name) {
    snprintf(snapshot_state.current_job, sizeof(snapshot_state.current_job),
             "%s", job_name && job_name[0] ? job_name : "Idle");
}

void profiler_record_render_ms(int render_ms) {
    int index = history_index == 0 ? PROFILER_FRAME_HISTORY - 1 : history_index - 1;
    render_ms_history[index] = render_ms;
    snapshot_state.render_avg_ms = history_average(render_ms_history);
    snapshot_state.render_peak_ms = history_peak(render_ms_history);
}

void profiler_record_phase(const char *phase_name, int elapsed_ms) {
    if (elapsed_ms > snapshot_state.slowest_phase_ms) {
        snprintf(snapshot_state.slowest_phase, sizeof(snapshot_state.slowest_phase), "%s", phase_name);
        snapshot_state.slowest_phase_ms = elapsed_ms;
    }
    if (strcmp(phase_name, "Resources") == 0) snapshot_state.resource_ms += elapsed_ms;
    else if (strcmp(phase_name, "Growth/Ports") == 0) snapshot_state.population_ms += elapsed_ms;
    else if (strcmp(phase_name, "Expansion") == 0) snapshot_state.expansion_ms += elapsed_ms;
    else if (strcmp(phase_name, "Plague") == 0) snapshot_state.plague_ms += elapsed_ms;
    else if (strcmp(phase_name, "Claim") == 0) snapshot_state.claim_ms += elapsed_ms;
    else if (strcmp(phase_name, "Diplomacy") == 0) snapshot_state.diplomacy_ms += elapsed_ms;
    else if (strcmp(phase_name, "Calendar") == 0) snapshot_state.war_ms += elapsed_ms;
}

void profiler_add_render_rebuild(ProfilerRenderLayer layer) {
    if (layer == PROFILER_RENDER_TERRAIN) snapshot_state.terrain_rebuild_count++;
    else if (layer == PROFILER_RENDER_POLITICAL) snapshot_state.political_rebuild_count++;
    else if (layer == PROFILER_RENDER_BORDER || layer == PROFILER_RENDER_COAST) snapshot_state.border_rebuild_count++;
    else if (layer == PROFILER_RENDER_LABEL) snapshot_state.label_rebuild_count++;
}

void profiler_add_gdi_recreate(void) {
    snapshot_state.gdi_bitmap_recreate_count++;
}

void profiler_add_scanned_regions(int count) {
    if (count > 0) snapshot_state.scanned_regions_this_month += count;
}

void profiler_add_scanned_tiles(int count) {
    if (count > 0) snapshot_state.scanned_tiles_this_month += count;
}

void profiler_add_claimed_regions(int count) {
    if (count > 0) snapshot_state.claimed_regions_this_month += count;
}

void profiler_add_expansion_civs_checked(int count) {
    if (count > 0) snapshot_state.expansion_civs_checked += count;
}

void profiler_add_expansion_claim_attempt(int count) {
    if (count > 0) snapshot_state.expansion_claims_attempted += count;
}

void profiler_add_expansion_claim_success(int count) {
    if (count > 0) snapshot_state.expansion_claims_succeeded += count;
}

void profiler_add_expansion_target_search_ms(int ms) {
    if (ms > 0) snapshot_state.expansion_target_search_ms += ms;
}

void profiler_add_claim_tiles_touched(int count) {
    if (count > 0) snapshot_state.claim_tiles_touched += count;
}

void profiler_add_maritime_path_search(int nodes) {
    snapshot_state.maritime_path_searches++;
    if (nodes > 0) snapshot_state.maritime_bfs_nodes += nodes;
}

void profiler_record_scheduler_step(int step_ms, int over_budget) {
    snapshot_state.scheduler_step_ms = step_ms;
    snapshot_state.scheduler_step_over_budget = over_budget;
}

void profiler_snapshot(RuntimeProfilerSnapshot *out) {
    if (out) *out = snapshot_state;
}
