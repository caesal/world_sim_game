#include "core/profiler.h"
#include "core/game_state.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#define PROFILER_FRAME_HISTORY 60
#define PROFILER_SLOW_CALL_MS 50
#define PROFILER_SPIKE_RING 64
#define PROFILER_SPIKE_TRACE_MS 16
#define PROFILER_SPIKE_RECENT_MS 10000
#define PROFILER_RENDER_PHASE_RING 64

#include "core/constants.h"
#include "core/sim_types.h"

typedef struct {
    DWORD tick;
    ProfilerSpikeEntry entry;
} ProfilerSpikeRecord;

typedef struct {
    DWORD tick;
    ProfilerRenderPhaseEntry entry;
} ProfilerRenderPhaseRecord;

static int frame_ms_history[PROFILER_FRAME_HISTORY];
static int sim_ms_history[PROFILER_FRAME_HISTORY];
static int render_ms_history[PROFILER_FRAME_HISTORY];
static int history_index;
static int history_count;
static ProfilerSpikeRecord spike_ring[PROFILER_SPIKE_RING];
static int spike_ring_head;
static int spike_ring_count;
static ProfilerRenderPhaseRecord render_phase_ring[PROFILER_RENDER_PHASE_RING];
static int render_phase_ring_head;
static int render_phase_ring_count;
static long long render_mode_sum_ms[PROFILER_RENDER_MODE_BUCKETS];
static DWORD last_spike_tick;
static RuntimeProfilerSnapshot snapshot_state;

extern Civilization civs[MAX_CIVS];
extern int civ_count;

static const char *spike_category_names[PROFILER_SPIKE_COUNT] = {
    "simulation",
    "presentation",
    "snapshot publish",
    "static cache",
    "city overlay",
    "label/cache",
    "expansion",
    "route/maritime",
    "annual diplomacy",
    "annual alliance"
};

long long profiler_now_us(void) {
    static LARGE_INTEGER frequency;
    LARGE_INTEGER now;

    if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&now);
    return now.QuadPart * 1000000LL / frequency.QuadPart;
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

int profiler_elapsed_ms_since_us(long long start_us) {
    long long elapsed = profiler_now_us() - start_us;
    return elapsed > 0 ? (int)((elapsed + 500) / 1000) : 0;
}

const char *profiler_spike_category_name(int category) {
    if (category < 0 || category >= PROFILER_SPIKE_COUNT) return "unknown";
    return spike_category_names[category];
}

static ProfilerSpikeCategory classify_spike_name(const char *name, ProfilerSpikeCategory fallback) {
    if (!name) return fallback;
    if (strstr(name, "Snapshot") || strstr(name, "snapshot")) return PROFILER_SPIKE_SNAPSHOT;
    if (strstr(name, "Static") || strstr(name, "static")) return PROFILER_SPIKE_STATIC_CACHE;
    if (strstr(name, "City overlay") || strstr(name, "city overlay")) return PROFILER_SPIKE_CITY_OVERLAY;
    if (strstr(name, "Label") || strstr(name, "label")) return PROFILER_SPIKE_LABEL_CACHE;
    if (strstr(name, "Expansion") || strstr(name, "expansion")) return PROFILER_SPIKE_EXPANSION;
    if (strstr(name, "route") || strstr(name, "Route") || strstr(name, "maritime") ||
        strstr(name, "Maritime") || strstr(name, "ports_") || strstr(name, "sea_lanes")) {
        return PROFILER_SPIKE_ROUTE_MARITIME;
    }
    if (strstr(name, "Diplomacy Year") || strstr(name, "diplomacy-year")) return PROFILER_SPIKE_ANNUAL_DIPLOMACY;
    if (strstr(name, "Alliance Year") || strstr(name, "alliance-year")) return PROFILER_SPIKE_ANNUAL_ALLIANCE;
    return fallback;
}

static void insert_recent_top(ProfilerSpikeEntry *top, int *count, ProfilerSpikeEntry entry) {
    int pos;
    int i;
    for (pos = 0; pos < *count && top[pos].duration_ms >= entry.duration_ms; pos++) {}
    if (pos >= 5) return;
    if (*count < 5) (*count)++;
    for (i = *count - 1; i > pos; i--) top[i] = top[i - 1];
    top[pos] = entry;
}

static void insert_recent_render_top(ProfilerRenderPhaseEntry *top, int *count,
                                     ProfilerRenderPhaseEntry entry) {
    int pos;
    int i;
    for (pos = 0; pos < *count && top[pos].duration_ms >= entry.duration_ms; pos++) {}
    if (pos >= 5) return;
    if (*count < 5) (*count)++;
    for (i = *count - 1; i > pos; i--) top[i] = top[i - 1];
    top[pos] = entry;
}

static int render_display_bucket(void) {
    return display_mode >= 0 && display_mode < PROFILER_RENDER_MODE_BUCKETS ?
           display_mode : 0;
}

void profiler_reset(void) {
    memset(frame_ms_history, 0, sizeof(frame_ms_history));
    memset(sim_ms_history, 0, sizeof(sim_ms_history));
    memset(render_ms_history, 0, sizeof(render_ms_history));
    memset(spike_ring, 0, sizeof(spike_ring));
    memset(render_phase_ring, 0, sizeof(render_phase_ring));
    memset(render_mode_sum_ms, 0, sizeof(render_mode_sum_ms));
    memset(&snapshot_state, 0, sizeof(snapshot_state));
    history_index = 0;
    history_count = 0;
    spike_ring_head = 0;
    spike_ring_count = 0;
    render_phase_ring_head = 0;
    render_phase_ring_count = 0;
    last_spike_tick = 0;
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

void profiler_note_presentation_state(int presented_year, int presented_month, int pending_months) {
    snapshot_state.profiler_presented_year = presented_year;
    snapshot_state.profiler_presented_month = presented_month;
    snapshot_state.profiler_presentation_pending = pending_months;
}

void profiler_note_static_cache_state(int needs_work, int presented_current, int presentable) {
    snapshot_state.profiler_static_needs_work = needs_work;
    snapshot_state.profiler_static_presented_current = presented_current;
    snapshot_state.profiler_static_presentable = presentable;
}

void profiler_set_current_job(const char *job_name) {
    snprintf(snapshot_state.current_job, sizeof(snapshot_state.current_job),
             "%s", job_name && job_name[0] ? job_name : "Idle");
}

void profiler_record_render_ms(int render_ms) {
    int index = history_index == 0 ? PROFILER_FRAME_HISTORY - 1 : history_index - 1;
    int bucket = render_display_bucket();
    render_ms_history[index] = render_ms;
    snapshot_state.render_avg_ms = history_average(render_ms_history);
    snapshot_state.render_peak_ms = history_peak(render_ms_history);
    if (render_ms > 0) {
        if (snapshot_state.render_mode_samples[bucket] < 1000000000) {
            snapshot_state.render_mode_samples[bucket]++;
            render_mode_sum_ms[bucket] += render_ms;
        }
        if (render_ms > snapshot_state.render_mode_peak_ms[bucket]) {
            snapshot_state.render_mode_peak_ms[bucket] = render_ms;
        }
        if (snapshot_state.render_mode_samples[bucket] > 0) {
            snapshot_state.render_mode_avg_ms[bucket] =
                (int)(render_mode_sum_ms[bucket] / snapshot_state.render_mode_samples[bucket]);
        }
    }
    profiler_record_spike_phase(PROFILER_SPIKE_PRESENTATION, "Render frame", render_ms);
}

void profiler_record_render_subphase(ProfilerRenderSubphase subphase,
                                     ProfilerSpikeCategory category,
                                     const char *phase_name, int elapsed_ms) {
    ProfilerRenderPhaseRecord *slot;
    ProfilerRenderPhaseEntry entry;
    const char *name;
    if (elapsed_ms <= 0) return;
    if (subphase < 0 || subphase >= PROFILER_RENDER_SUB_COUNT) subphase = PROFILER_RENDER_SUB_STATIC_CACHE;
    name = phase_name && phase_name[0] ? phase_name : "Render subphase";
    if (elapsed_ms > snapshot_state.render_subphase_peak_ms[subphase]) {
        snapshot_state.render_subphase_peak_ms[subphase] = elapsed_ms;
        snapshot_state.render_subphase_peak_display[subphase] = display_mode;
        snprintf(snapshot_state.render_subphase_peak_phase[subphase],
                 sizeof(snapshot_state.render_subphase_peak_phase[subphase]),
                 "%s", name);
    }
    if (elapsed_ms >= PROFILER_SPIKE_TRACE_MS) {
        memset(&entry, 0, sizeof(entry));
        snprintf(entry.phase, sizeof(entry.phase), "%s", name);
        entry.duration_ms = elapsed_ms;
        entry.display_mode = display_mode;
        slot = &render_phase_ring[render_phase_ring_head];
        slot->tick = GetTickCount();
        slot->entry = entry;
        render_phase_ring_head = (render_phase_ring_head + 1) % PROFILER_RENDER_PHASE_RING;
        if (render_phase_ring_count < PROFILER_RENDER_PHASE_RING) render_phase_ring_count++;
    }
    profiler_record_spike_phase(category, name, elapsed_ms);
}

void profiler_record_spike_phase(ProfilerSpikeCategory category, const char *phase_name, int elapsed_ms) {
    ProfilerSpikeEntry entry;
    ProfilerSpikeRecord *slot;
    DWORD now;

    if (elapsed_ms <= 0) return;
    category = classify_spike_name(phase_name, category);
    if (category < 0 || category >= PROFILER_SPIKE_COUNT) category = PROFILER_SPIKE_SIMULATION;
    if (elapsed_ms > snapshot_state.spike_category_peak_ms[category]) {
        snapshot_state.spike_category_peak_ms[category] = elapsed_ms;
        snprintf(snapshot_state.spike_category_peak_phase[category],
                 sizeof(snapshot_state.spike_category_peak_phase[category]),
                 "%s", phase_name && phase_name[0] ? phase_name : profiler_spike_category_name(category));
    }
    if (elapsed_ms < PROFILER_SPIKE_TRACE_MS) return;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.phase, sizeof(entry.phase), "%s",
             phase_name && phase_name[0] ? phase_name : profiler_spike_category_name(category));
    entry.duration_ms = elapsed_ms;
    entry.sim_year = year;
    entry.sim_month = month;
    entry.presented_year = snapshot_state.profiler_presented_year;
    entry.presented_month = snapshot_state.profiler_presented_month;
    entry.auto_run_active = auto_run;
    entry.speed_index = speed_index;
    entry.display_mode = display_mode;
    entry.pending_presented_months = snapshot_state.profiler_presentation_pending;
    entry.static_needs_work = snapshot_state.profiler_static_needs_work;
    entry.static_presented_current = snapshot_state.profiler_static_presented_current;
    entry.static_presentable = snapshot_state.profiler_static_presentable;
    now = GetTickCount();
    slot = &spike_ring[spike_ring_head];
    slot->tick = now;
    slot->entry = entry;
    spike_ring_head = (spike_ring_head + 1) % PROFILER_SPIKE_RING;
    if (spike_ring_count < PROFILER_SPIKE_RING) spike_ring_count++;
    snapshot_state.last_spike = entry;
    last_spike_tick = now;
}

void profiler_record_phase(const char *phase_name, int elapsed_ms) {
    const char *name = phase_name ? phase_name : "unknown";
    ProfilerSpikeCategory category = classify_spike_name(name, PROFILER_SPIKE_SIMULATION);
    profiler_record_spike_phase(category, name, elapsed_ms);
    if (elapsed_ms > snapshot_state.slowest_phase_ms) {
        snprintf(snapshot_state.slowest_phase, sizeof(snapshot_state.slowest_phase), "%s", name);
        snapshot_state.slowest_phase_ms = elapsed_ms;
    }
    if (strcmp(name, "Resources") == 0) snapshot_state.resource_ms += elapsed_ms;
    else if (strcmp(name, "Growth/Ports") == 0) snapshot_state.population_ms += elapsed_ms;
    else if (strcmp(name, "Expansion") == 0) snapshot_state.expansion_ms += elapsed_ms;
    else if (strcmp(name, "Plague") == 0) snapshot_state.plague_ms += elapsed_ms;
    else if (strcmp(name, "Claim") == 0) snapshot_state.claim_ms += elapsed_ms;
    else if (strcmp(name, "Diplomacy") == 0) snapshot_state.diplomacy_ms += elapsed_ms;
    else if (strcmp(name, "Calendar") == 0) snapshot_state.war_ms += elapsed_ms;
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
    ProfilerSpikeCategory category;

    category = classify_spike_name(function_name, PROFILER_SPIKE_SIMULATION);
    profiler_record_spike_phase(category, function_name, elapsed_ms);
    if (elapsed_ms < PROFILER_SLOW_CALL_MS) return elapsed_ms;
    if (elapsed_ms > snapshot_state.last_slow_call_ms) {
        snprintf(snapshot_state.last_slow_call, sizeof(snapshot_state.last_slow_call),
                 "%s %d ms civ %d region %d reg %d tiles %d touch %d paths %d nodes %d",
                 function_name ? function_name : "unknown", elapsed_ms, civ_id, region_id,
                 regions, tiles, touched, paths, nodes);
        snapshot_state.last_slow_call_ms = elapsed_ms;
    }
    if (log_event) {
        snprintf(text, sizeof(text), "%s", function_name ? function_name : "unknown");
        event_log_push_structured(EVENT_TYPE_PERFORMANCE_SLOW_CALL, EVENT_SEVERITY_WARNING,
                                  civ_id, -1, region_id, -1, elapsed_ms, tiles, text);
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
    profiler_record_spike_phase(PROFILER_SPIKE_ROUTE_MARITIME, "sea_lanes_rebuild", rebuild_ms);
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
    profiler_record_spike_phase(PROFILER_SPIKE_SIMULATION,
                                snapshot_state.current_job[0] ? snapshot_state.current_job : "Scheduler step",
                                step_ms);
}

void profiler_snapshot(RuntimeProfilerSnapshot *out) {
    RuntimeProfilerSnapshot copy;
    DWORD now;
    int i;
    if (!out) return;
    copy = snapshot_state;
    now = GetTickCount();
    copy.recent_spike_count = 0;
    if (last_spike_tick != 0) copy.last_spike.age_ms = (int)(now - last_spike_tick);
    for (i = 0; i < spike_ring_count; i++) {
        int index = (spike_ring_head - 1 - i + PROFILER_SPIKE_RING) % PROFILER_SPIKE_RING;
        DWORD tick = spike_ring[index].tick;
        ProfilerSpikeEntry entry = spike_ring[index].entry;
        int age;
        if (tick == 0) continue;
        age = (int)(now - tick);
        if (age > PROFILER_SPIKE_RECENT_MS) continue;
        entry.age_ms = age;
        insert_recent_top(copy.recent_spikes, &copy.recent_spike_count, entry);
    }
    copy.recent_render_phase_count = 0;
    for (i = 0; i < render_phase_ring_count; i++) {
        int index = (render_phase_ring_head - 1 - i + PROFILER_RENDER_PHASE_RING) % PROFILER_RENDER_PHASE_RING;
        DWORD tick = render_phase_ring[index].tick;
        ProfilerRenderPhaseEntry entry = render_phase_ring[index].entry;
        int age;
        if (tick == 0) continue;
        age = (int)(now - tick);
        if (age > PROFILER_SPIKE_RECENT_MS) continue;
        entry.age_ms = age;
        insert_recent_render_top(copy.recent_render_phases, &copy.recent_render_phase_count, entry);
    }
    *out = copy;
}
