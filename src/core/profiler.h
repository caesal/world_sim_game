#ifndef WORLD_SIM_PROFILER_H
#define WORLD_SIM_PROFILER_H

typedef enum {
    PROFILER_RENDER_TERRAIN,
    PROFILER_RENDER_POLITICAL,
    PROFILER_RENDER_COAST,
    PROFILER_RENDER_BORDER,
    PROFILER_RENDER_LABEL,
    PROFILER_RENDER_COUNT
} ProfilerRenderLayer;

typedef enum {
    PROFILER_SPIKE_SIMULATION,
    PROFILER_SPIKE_PRESENTATION,
    PROFILER_SPIKE_SNAPSHOT,
    PROFILER_SPIKE_STATIC_CACHE,
    PROFILER_SPIKE_CITY_OVERLAY,
    PROFILER_SPIKE_LABEL_CACHE,
    PROFILER_SPIKE_EXPANSION,
    PROFILER_SPIKE_ROUTE_MARITIME,
    PROFILER_SPIKE_ANNUAL_DIPLOMACY,
    PROFILER_SPIKE_ANNUAL_ALLIANCE,
    PROFILER_SPIKE_COUNT
} ProfilerSpikeCategory;

typedef enum {
    PROFILER_RENDER_SUB_STATIC_SCENE,
    PROFILER_RENDER_SUB_STATIC_CACHE,
    PROFILER_RENDER_SUB_PHYSICAL,
    PROFILER_RENDER_SUB_FILL,
    PROFILER_RENDER_SUB_BORDERS,
    PROFILER_RENDER_SUB_CITY_OVERLAY,
    PROFILER_RENDER_SUB_COUNTRY_LABELS,
    PROFILER_RENDER_SUB_ALLIANCE_LABELS,
    PROFILER_RENDER_SUB_ROUTE_OVERLAY,
    PROFILER_RENDER_SUB_HIGHLIGHT_OVERLAY,
    PROFILER_RENDER_SUB_MAP_LEGEND,
    PROFILER_RENDER_SUB_SIDE_PANEL,
    PROFILER_RENDER_SUB_TOP_BOTTOM_BAR,
    PROFILER_RENDER_SUB_BACKBUFFER,
    PROFILER_RENDER_SUB_COUNT
} ProfilerRenderSubphase;

typedef struct {
    long long start_us;
    int regions_scanned;
    int tiles_scanned;
    int claim_tiles_touched;
    int maritime_path_searches;
    int maritime_bfs_nodes;
} ProfilerCallTrace;

typedef struct {
    char phase[32];
    int duration_ms;
    int sim_year;
    int sim_month;
    int presented_year;
    int presented_month;
    int auto_run_active;
    int speed_index;
    int display_mode;
    int pending_presented_months;
    int static_needs_work;
    int static_presented_current;
    int static_presentable;
    int age_ms;
} ProfilerSpikeEntry;

#define PROFILER_RENDER_MODE_BUCKETS 8

typedef struct {
    char phase[32];
    int duration_ms;
    int display_mode;
    int age_ms;
} ProfilerRenderPhaseEntry;

typedef struct {
    int frame_avg_ms;
    int frame_peak_ms;
    int sim_avg_ms;
    int sim_peak_ms;
    int render_avg_ms;
    int render_peak_ms;
    int sim_budget_ms;
    int sim_used_ms;
    int actual_ms_per_month;
    int pending_months;
    int overloaded;
    char current_job[32];
    char slowest_phase[32];
    int slowest_phase_ms;
    int resource_ms;
    int population_ms;
    int expansion_ms;
    int claim_ms;
    int diplomacy_ms;
    int war_ms;
    int plague_ms;
    int terrain_rebuild_count;
    int political_rebuild_count;
    int border_rebuild_count;
    int label_rebuild_count;
    int gdi_bitmap_recreate_count;
    int claimed_regions_this_month;
    int scanned_regions_this_month;
    int scanned_tiles_this_month;
    int expansion_civs_checked;
    int expansion_claims_attempted;
    int expansion_claims_succeeded;
    int expansion_target_search_ms;
    int claim_tiles_touched;
    int maritime_path_searches;
    int maritime_bfs_nodes;
    int sea_lane_rebuild_ms;
    int sea_lane_count;
    int sea_lane_merged_routes;
    int sea_lane_skipped_routes;
    int route_land_reject_count;
    int contour_rebuild_ms;
    int contour_path_count;
    int terrain_cache_width;
    int terrain_cache_height;
    int terrain_stretch_mode;
    char terrain_render_mode[24];
    int scheduler_step_ms;
    int scheduler_step_over_budget;
    int last_slow_call_ms;
    char last_slow_call[192];
    int profiler_presented_year;
    int profiler_presented_month;
    int profiler_presentation_pending;
    int profiler_static_needs_work;
    int profiler_static_presented_current;
    int profiler_static_presentable;
    ProfilerSpikeEntry last_spike;
    ProfilerSpikeEntry recent_spikes[5];
    int recent_spike_count;
    int spike_category_peak_ms[PROFILER_SPIKE_COUNT];
    char spike_category_peak_phase[PROFILER_SPIKE_COUNT][32];
    int render_mode_avg_ms[PROFILER_RENDER_MODE_BUCKETS];
    int render_mode_peak_ms[PROFILER_RENDER_MODE_BUCKETS];
    int render_mode_samples[PROFILER_RENDER_MODE_BUCKETS];
    int render_subphase_peak_ms[PROFILER_RENDER_SUB_COUNT];
    int render_subphase_peak_display[PROFILER_RENDER_SUB_COUNT];
    char render_subphase_peak_phase[PROFILER_RENDER_SUB_COUNT][32];
    ProfilerRenderPhaseEntry recent_render_phases[5];
    int recent_render_phase_count;
} RuntimeProfilerSnapshot;

void profiler_reset(void);
void profiler_begin_month(void);
void profiler_record_frame(int frame_ms, int sim_budget_ms, int sim_used_ms,
                           int actual_ms_per_month, int pending_months, int overloaded);
void profiler_set_current_job(const char *job_name);
void profiler_record_render_ms(int render_ms);
void profiler_record_render_subphase(ProfilerRenderSubphase subphase,
                                     ProfilerSpikeCategory category,
                                     const char *phase_name, int elapsed_ms);
void profiler_record_phase(const char *phase_name, int elapsed_ms);
void profiler_record_spike_phase(ProfilerSpikeCategory category, const char *phase_name, int elapsed_ms);
void profiler_note_presentation_state(int presented_year, int presented_month, int pending_months);
void profiler_note_static_cache_state(int needs_work, int presented_current, int presentable);
const char *profiler_spike_category_name(int category);
void profiler_add_render_rebuild(ProfilerRenderLayer layer);
void profiler_add_gdi_recreate(void);
void profiler_add_scanned_regions(int count);
void profiler_add_scanned_tiles(int count);
void profiler_add_claimed_regions(int count);
void profiler_add_expansion_civs_checked(int count);
void profiler_add_expansion_claim_attempt(int count);
void profiler_add_expansion_claim_success(int count);
void profiler_add_expansion_target_search_ms(int ms);
void profiler_add_claim_tiles_touched(int count);
void profiler_add_maritime_path_search(int nodes);
void profiler_record_sea_lanes(int rebuild_ms, int lanes, int merged, int skipped, int rejected);
void profiler_record_contours(int rebuild_ms, int paths);
void profiler_record_terrain_present(const char *mode, int width, int height, int stretch_mode);
void profiler_record_scheduler_step(int step_ms, int over_budget);
ProfilerCallTrace profiler_call_begin(void);
int profiler_call_end(const char *function_name, int civ_id, int region_id,
                      ProfilerCallTrace trace);
int profiler_call_end_quiet(const char *function_name, int civ_id, int region_id,
                            ProfilerCallTrace trace);
void profiler_snapshot(RuntimeProfilerSnapshot *out);

#endif
