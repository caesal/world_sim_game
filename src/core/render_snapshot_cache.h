#ifndef WORLD_SIM_RENDER_SNAPSHOT_CACHE_H
#define WORLD_SIM_RENDER_SNAPSHOT_CACHE_H

#include "core/render_snapshot.h"

void render_snapshot_cache_reset(void);
void render_snapshot_cache_begin_snapshot_copy(void);
void render_snapshot_cache_update_budgeted(int city_budget, int pair_budget,
                                           int refresh_lanes, int refresh_plague);
void render_snapshot_cache_update_all(void);
int render_snapshot_cache_dirty_count(void);

int render_snapshot_cache_city_summary(int city_id, int key, RegionSummary *region,
                                       PopulationSummary *population);
int render_snapshot_cache_diplomacy_pair(int civ_a, int civ_b, int key,
                                         SnapshotDiplomacyRelation *relation,
                                         SnapshotWar *war, int *front_flags,
                                         int *peace_pressure);
int render_snapshot_cache_lanes(int key, const SnapshotSeaLane **lanes, int *count);
int render_snapshot_cache_plague_civ(int civ_id, int key, int *active_count,
                                     int *months_left, int *peak_severity,
                                     int *deaths_total);
int render_snapshot_cache_plague_city(int city_id, int key, int *active,
                                      int *severity, int *months_left,
                                      int *deaths_total);
int render_snapshot_cache_plague_lane(int lane_id, int key, int *exposure);
void render_snapshot_cache_note_city_fallback(void);
void render_snapshot_cache_note_diplomacy_fallback(void);
void render_snapshot_cache_note_plague_fallback(void);

const char *render_snapshot_cache_city_summary_debug(void);
const char *render_snapshot_cache_diplomacy_debug(void);
const char *render_snapshot_cache_plague_debug(void);
const char *render_snapshot_cache_lane_debug(void);

#endif
