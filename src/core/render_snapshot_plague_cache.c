#include "core/render_snapshot_plague_cache.h"

#include "core/game_types.h"
#include "core/render_snapshot_cache.h"
#include "core/render_snapshot_keys.h"
#include "core/render_snapshot_plague_impact.h"
#include "data/plague_names.h"
#include "sim/plague.h"
#include "sim/plague_disorder.h"
#include "sim/plague_metrics.h"
#include "sim/plague_rules.h"
#include "sim/plague_state.h"
#include "sim/sea_lanes.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int active_count;
    int months_left;
    int peak_severity;
    int deaths_total;
} PlagueCivCache;

typedef struct {
    int active;
    int severity;
    int months_left;
    int deaths_total;
} PlagueCityCache;

static PlagueCivCache plague_civ_cache[MAX_CIVS];
static PlagueCityCache plague_city_cache[MAX_CITIES];
static int plague_lane_exposure[MAX_SEA_LANES];
static int plague_cache_key;
static int plague_cache_valid;
static int plague_cache_active;
static PlagueStateView plague_state_cache;
static PlagueMetricsSnapshot plague_metrics_cache;
static SnapshotPlagueNames plague_names_cache;
static SnapshotPlagueImpact plague_impact_cache;
static unsigned int plague_impact_refresh_count;
static int copy_plague_cached;
static int copy_plague_stale;
static int copy_plague_fallback;

static void refresh_plague(int key) {
    const SeaLane *lanes;
    int absolute_month = plague_rules_absolute_month(year, month);
    int lane_count, i;
    memset(plague_civ_cache, 0, sizeof(plague_civ_cache));
    memset(plague_city_cache, 0, sizeof(plague_city_cache));
    memset(plague_lane_exposure, 0, sizeof(plague_lane_exposure));
    memset(&plague_names_cache, 0, sizeof(plague_names_cache));
    plague_state_build_view(absolute_month, &plague_state_cache);
    plague_metrics_snapshot(&plague_metrics_cache);
    render_snapshot_plague_impact_build(&plague_impact_cache, absolute_month);
    plague_impact_cache.refresh_count = ++plague_impact_refresh_count;
    plague_disorder_max_current_target(&plague_state_cache.disorder_current,
                                        &plague_state_cache.disorder_target);
    if (plague_state_cache.episode.active) {
        PlagueSizeRules rules;
        int duration = max(0, absolute_month - plague_state_cache.episode.start_month);
        plague_state_cache.projected_immunity_percent =
            plague_rules_immunity_percent_for_duration(duration);
        plague_rules_next_immunity_tier(duration,
                                        &plague_state_cache.next_immunity_percent,
                                        &plague_state_cache.months_to_next_immunity);
        if (plague_rules_size_values(plague_state_cache.episode.size, &rules)) {
            plague_state_cache.maximum_generation_index = rules.maximum_generation;
            plague_state_cache.reachable_generation_layers =
                rules.maximum_generation + 1;
        }
        plague_names_format(plague_state_cache.episode.name_id,
                            plague_state_cache.episode.name_cycle, 0,
                            plague_names_cache.active_en,
                            sizeof(plague_names_cache.active_en));
        plague_names_format(plague_state_cache.episode.name_id,
                            plague_state_cache.episode.name_cycle, 1,
                            plague_names_cache.active_zh,
                            sizeof(plague_names_cache.active_zh));
    }
    for (i = 0; i < plague_state_cache.recent_history_count; i++) {
        const PlagueEpisodeHistory *history = &plague_state_cache.recent_history[i];
        plague_names_format(history->name_id, history->name_cycle, 0,
                            plague_names_cache.history_en[i],
                            sizeof(plague_names_cache.history_en[i]));
        plague_names_format(history->name_id, history->name_cycle, 1,
                            plague_names_cache.history_zh[i],
                            sizeof(plague_names_cache.history_zh[i]));
    }
    plague_cache_active = 0;
    for (i = 0; i < city_count && i < MAX_CITIES; i++) {
        PlagueCityCache *city = &plague_city_cache[i];
        int owner;
        if (!cities[i].alive) continue;
        city->active = plague_city_active(i);
        city->severity = plague_city_severity(i);
        city->months_left = plague_city_months_left(i);
        city->deaths_total = plague_city_deaths_total(i);
        owner = cities[i].owner;
        if (owner < 0 || owner >= civ_count || owner >= MAX_CIVS) continue;
        plague_civ_cache[owner].deaths_total += city->deaths_total;
        if (!city->active) continue;
        plague_cache_active = 1;
        plague_civ_cache[owner].active_count++;
        plague_civ_cache[owner].peak_severity = max(plague_civ_cache[owner].peak_severity, city->severity);
        plague_civ_cache[owner].months_left = max(plague_civ_cache[owner].months_left, city->months_left);
    }
    lanes = sea_lanes_get(&lane_count);
    lane_count = clamp(lane_count, 0, MAX_SEA_LANES);
    for (i = 0; i < lane_count; i++) plague_lane_exposure[i] = lanes[i].exposure;
    plague_cache_key = key;
    plague_cache_valid = 1;
}

void render_snapshot_plague_cache_reset(void) {
    memset(plague_civ_cache, 0, sizeof(plague_civ_cache));
    memset(plague_city_cache, 0, sizeof(plague_city_cache));
    memset(plague_lane_exposure, 0, sizeof(plague_lane_exposure));
    memset(&plague_state_cache, 0, sizeof(plague_state_cache));
    memset(&plague_metrics_cache, 0, sizeof(plague_metrics_cache));
    memset(&plague_names_cache, 0, sizeof(plague_names_cache));
    memset(&plague_impact_cache, 0, sizeof(plague_impact_cache));
    plague_impact_refresh_count = 0;
    plague_cache_key = 0;
    plague_cache_valid = 0;
    plague_cache_active = 0;
    copy_plague_cached = 0;
    copy_plague_stale = 0;
    copy_plague_fallback = 0;
}

void render_snapshot_plague_cache_begin_snapshot_copy(void) {
    copy_plague_cached = 0;
    copy_plague_stale = 0;
    copy_plague_fallback = 0;
}

int render_snapshot_plague_cache_update_if_dirty(int key) {
    if (!render_snapshot_plague_cache_is_dirty(key)) return 0;
    refresh_plague(key);
    return 1;
}

void render_snapshot_plague_cache_refresh(int key) {
    refresh_plague(key);
}

void render_snapshot_plague_cache_invalidate(void) {
    plague_cache_valid = 0;
    plague_cache_active = 0;
}

int render_snapshot_plague_cache_is_dirty(int key) {
    return !plague_cache_valid || plague_cache_key != key;
}

int render_snapshot_cache_plague_civ(int civ_id, int key, int *active_count,
                                     int *months_left, int *peak_severity,
                                     int *deaths_total) {
    PlagueCivCache *entry;
    if (civ_id < 0 || civ_id >= civ_count || civ_id >= MAX_CIVS) return 0;
    if (!plague_cache_valid || plague_cache_key != key) { copy_plague_stale++; return 0; }
    entry = &plague_civ_cache[civ_id];
    if (active_count) *active_count = entry->active_count;
    if (months_left) *months_left = entry->months_left;
    if (peak_severity) *peak_severity = entry->peak_severity;
    if (deaths_total) *deaths_total = entry->deaths_total;
    copy_plague_cached++;
    return 1;
}

int render_snapshot_cache_plague_city(int city_id, int key, int *active,
                                      int *severity, int *months_left,
                                      int *deaths_total) {
    PlagueCityCache *entry;
    if (city_id < 0 || city_id >= city_count || city_id >= MAX_CITIES) return 0;
    if (!plague_cache_valid || plague_cache_key != key) { copy_plague_stale++; return 0; }
    entry = &plague_city_cache[city_id];
    if (active) *active = entry->active;
    if (severity) *severity = entry->severity;
    if (months_left) *months_left = entry->months_left;
    if (deaths_total) *deaths_total = entry->deaths_total;
    copy_plague_cached++;
    return 1;
}

int render_snapshot_cache_plague_lane(int lane_id, int key, int *exposure) {
    if (lane_id < 0 || lane_id >= MAX_SEA_LANES) return 0;
    if (!plague_cache_valid || plague_cache_key != key) { copy_plague_stale++; return 0; }
    if (exposure) *exposure = plague_lane_exposure[lane_id];
    copy_plague_cached++;
    return 1;
}

int render_snapshot_cache_plague_summary(int key, PlagueStateView *state,
                                         PlagueMetricsSnapshot *metrics,
                                         SnapshotPlagueNames *names) {
    if (!plague_cache_valid || plague_cache_key != key) {
        copy_plague_stale++;
        return 0;
    }
    if (state) *state = plague_state_cache;
    if (metrics) *metrics = plague_metrics_cache;
    if (names) *names = plague_names_cache;
    copy_plague_cached++;
    return 1;
}

int render_snapshot_cache_plague_impact(int key, SnapshotPlagueImpact *impact) {
    if (!plague_cache_valid || plague_cache_key != key) {
        copy_plague_stale++;
        return 0;
    }
    if (impact) *impact = plague_impact_cache;
    copy_plague_cached++;
    return 1;
}

void render_snapshot_cache_note_plague_fallback(void) {
    copy_plague_fallback++;
}

const char *render_snapshot_cache_plague_debug(void) {
    static char text[128];
    int lane_key = render_snapshot_lanes_revision_key();
    int dirty = !plague_cache_valid || plague_cache_key != render_snapshot_plague_revision_key(lane_key);
    snprintf(text, sizeof(text),
             "cached %d / stale %d / fallback %d / dirty %d / active %d / impact %u:%d:%d",
             copy_plague_cached, copy_plague_stale, copy_plague_fallback, dirty,
             plague_cache_active, plague_impact_refresh_count,
             plague_impact_cache.candidate_city_count,
             plague_impact_cache.country_sort_comparison_count);
    return text;
}
