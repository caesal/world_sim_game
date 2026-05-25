#ifndef WORLD_SIM_PLAGUE_PERF_H
#define WORLD_SIM_PLAGUE_PERF_H

int plague_perf_system_enabled(void);
int plague_perf_map_visuals_enabled(void);
int plague_perf_visuals_allowed(void);
void plague_perf_set_system_enabled(int enabled);
void plague_perf_set_map_visuals_enabled(int enabled);
void plague_perf_toggle_system(void);
void plague_perf_toggle_map_visuals(void);
void plague_perf_begin_frame(void);
void plague_perf_note_sim_skipped(int skipped);
void plague_perf_note_visual_skipped(int skipped);
void plague_perf_note_invalidation_suppressed(int suppressed);
int plague_perf_sim_skipped(void);
int plague_perf_visual_skipped(void);
int plague_perf_invalidation_suppressed(void);

#endif
