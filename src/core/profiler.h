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
    int scheduler_step_ms;
    int scheduler_step_over_budget;
} RuntimeProfilerSnapshot;

void profiler_reset(void);
void profiler_begin_month(void);
void profiler_record_frame(int frame_ms, int sim_budget_ms, int sim_used_ms,
                           int actual_ms_per_month, int pending_months, int overloaded);
void profiler_set_current_job(const char *job_name);
void profiler_record_render_ms(int render_ms);
void profiler_record_phase(const char *phase_name, int elapsed_ms);
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
void profiler_record_scheduler_step(int step_ms, int over_budget);
void profiler_snapshot(RuntimeProfilerSnapshot *out);

#endif
