#include "core/profiler.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#define PROFILER_FRAME_HISTORY 60
#define PROFILER_SLOW_CALL_MS 50

#include "core/constants.h"
#include "core/sim_types.h"

static int frame_ms_history[PROFILER_FRAME_HISTORY];
static int sim_ms_history[PROFILER_FRAME_HISTORY];
static int render_ms_history[PROFILER_FRAME_HISTORY];
static int history_index;
static int history_count;
static RuntimeProfilerSnapshot snapshot_state;

extern Civilization civs[MAX_CIVS];
extern int civ_count;
extern int city_count;
extern int region_count;
void event_log_push(const char *text);

static long long profiler_now_us(void) {
    static LARGE_INTEGER frequency;
    LARGE_INTEGER now;

    if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&now);
    return now.QuadPart * 1000000LL / frequency.QuadPart;
}

static int active_civ_count(void) {
    int i;
    int count = 0;

    for (i = 0; i < civ_count; i++) {
        if (civs[i].alive) count++;
    }
    return count;
}

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
    snapshot_state.sea_lane_rebuild_ms = 0;
    snapshot_state.sea_lane_count = 0;
    snapshot_state.sea_lane_merged_routes = 0;
    snapshot_state.sea_lane_skipped_routes = 0;
    snapshot_state.route_land_reject_count = 0;
    snapshot_state.contour_rebuild_ms = 0;
    snapshot_state.contour_path_count = 0;
    snapshot_state.last_slow_call_ms = 0;
    snapshot_state.last_slow_call[0] = '\0';
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

ProfilerCallTrace profiler_call_begin(void) {
    ProfilerCallTrace trace;

    trace.start_us = profiler_now_us();
    trace.regions_scanned = snapshot_state.scanned_regions_this_month;
    trace.tiles_scanned = snapshot_state.scanned_tiles_this_month;
    trace.claim_tiles_touched = snapshot_state.claim_tiles_touched;
    trace.maritime_path_searches = snapshot_state.maritime_path_searches;
    trace.maritime_bfs_nodes = snapshot_state.maritime_bfs_nodes;
    return trace;
}

static int profiler_call_finish(const char *function_name, int civ_id, int region_id,
                                ProfilerCallTrace trace, int log_event) {
    int elapsed_ms = (int)((profiler_now_us() - trace.start_us + 999) / 1000);
    int regions = snapshot_state.scanned_regions_this_month - trace.regions_scanned;
    int tiles = snapshot_state.scanned_tiles_this_month - trace.tiles_scanned;
    int touched = snapshot_state.claim_tiles_touched - trace.claim_tiles_touched;
    int paths = snapshot_state.maritime_path_searches - trace.maritime_path_searches;
    int nodes = snapshot_state.maritime_bfs_nodes - trace.maritime_bfs_nodes;
    char text[EVENT_LOG_LEN];

    if (elapsed_ms < PROFILER_SLOW_CALL_MS) return elapsed_ms;
    if (elapsed_ms > snapshot_state.last_slow_call_ms) {
        snprintf(snapshot_state.last_slow_call, sizeof(snapshot_state.last_slow_call),
                 "%s %d ms civ %d region %d reg %d tiles %d touch %d paths %d nodes %d",
                 function_name ? function_name : "unknown", elapsed_ms, civ_id, region_id,
                 regions, tiles, touched, paths, nodes);
        snapshot_state.last_slow_call_ms = elapsed_ms;
    }
    if (log_event) {
        snprintf(text, sizeof(text),
                 "[Performance] Slow %s: %d ms civ=%d region=%d regions=%d tiles=%d touched=%d paths=%d nodes=%d active=%d cities=%d region_count=%d",
                 function_name ? function_name : "unknown", elapsed_ms, civ_id, region_id,
                 regions, tiles, touched, paths, nodes, active_civ_count(), city_count, region_count);
        event_log_push(text);
    }
    return elapsed_ms;
}

int profiler_call_end(const char *function_name, int civ_id, int region_id,
                      ProfilerCallTrace trace) {
    return profiler_call_finish(function_name, civ_id, region_id, trace, 1);
}

int profiler_call_end_quiet(const char *function_name, int civ_id, int region_id,
                            ProfilerCallTrace trace) {
    return profiler_call_finish(function_name, civ_id, region_id, trace, 0);
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

void profiler_record_sea_lanes(int rebuild_ms, int lanes, int merged, int skipped, int rejected) {
    snapshot_state.sea_lane_rebuild_ms = rebuild_ms;
    snapshot_state.sea_lane_count = lanes;
    snapshot_state.sea_lane_merged_routes = merged;
    snapshot_state.sea_lane_skipped_routes = skipped;
    snapshot_state.route_land_reject_count = rejected;
}

void profiler_record_contours(int rebuild_ms, int paths) {
    snapshot_state.contour_rebuild_ms += rebuild_ms;
    snapshot_state.contour_path_count += paths;
}

void profiler_record_terrain_present(const char *mode, int width, int height, int stretch_mode) {
    snprintf(snapshot_state.terrain_render_mode, sizeof(snapshot_state.terrain_render_mode),
             "%s", mode ? mode : "unknown");
    snapshot_state.terrain_cache_width = width;
    snapshot_state.terrain_cache_height = height;
    snapshot_state.terrain_stretch_mode = stretch_mode;
}

void profiler_record_scheduler_step(int step_ms, int over_budget) {
    snapshot_state.scheduler_step_ms = step_ms;
    snapshot_state.scheduler_step_over_budget = over_budget;
}

void profiler_snapshot(RuntimeProfilerSnapshot *out) {
    if (out) *out = snapshot_state;
}
