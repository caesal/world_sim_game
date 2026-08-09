#ifndef WORLD_SIM_DECISION_SNAPSHOT_CACHE_H
#define WORLD_SIM_DECISION_SNAPSHOT_CACHE_H

#include <stdint.h>

#include "sim/decision_snapshot.h"

#define DECISION_SNAPSHOT_CACHE_SLICE_MAX_CIVS 8
#define DECISION_SNAPSHOT_CACHE_SLICE_MAX_US INT64_C(2000)

typedef struct {
    uint64_t published_revision;
    uint64_t building_revision;
    uint64_t total_calculation_count;
    int published_year;
    int published_month;
    int published_expected_count;
    int published_built_count;
    int published_count;
    int building_active;
    int building_year;
    int building_month;
    int building_expected_count;
    int building_built_count;
    int dirty_count;
    int last_slice_count;
    int64_t last_slice_us;
    int restart_count;
    int cancellation_count;
    int uid_mismatch_count;
} DecisionSnapshotCacheDiagnostics;

void decision_snapshot_cache_reset(void);
void decision_snapshot_cache_begin_generation(void);
int decision_snapshot_cache_service_slice(void);
int decision_snapshot_cache_seed_complete(void);
void decision_snapshot_cache_cancel_building(void);
void decision_snapshot_cache_mark_dirty(int civ_id);
void decision_snapshot_cache_mark_all_dirty(void);

int decision_snapshot_cached(int civ_id, DecisionSnapshot *out);
uint64_t decision_snapshot_cache_published_revision(void);
int decision_snapshot_cache_valid_count(void);
int decision_snapshot_cache_dirty_count(void);
int decision_snapshot_cache_last_update_ms(void);
int decision_snapshot_cache_last_update_count(void);
int64_t decision_snapshot_cache_last_update_us(void);
uint64_t decision_snapshot_cache_total_calculation_count(void);
void decision_snapshot_cache_get_diagnostics(DecisionSnapshotCacheDiagnostics *out);

#endif
