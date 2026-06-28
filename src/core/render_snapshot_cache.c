#include "core/render_snapshot_cache.h"

#include "core/dirty_flags.h"
#include "core/render_snapshot_keys.h"
#include "sim/diplomacy.h"
#include "sim/diplomacy_relation_score.h"
#include "sim/diplomacy_stability.h"
#include "sim/plague.h"
#include "sim/population.h"
#include "sim/sea_lanes.h"
#include "sim/simulation.h"
#include "sim/war.h"
#include "sim/alliance_military.h"
#include "sim/war_front.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int valid;
    int key;
    RegionSummary region;
    PopulationSummary population;
} CitySummaryCache;

typedef struct {
    int valid;
    int key;
    SnapshotDiplomacyRelation relation;
    SnapshotWar war;
    int front_flags;
    int peace_pressure;
} DiplomacyPairCache;

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

static CitySummaryCache city_cache[MAX_CITIES];
static DiplomacyPairCache pair_cache[MAX_CIVS][MAX_CIVS];
static SnapshotSeaLane lane_cache[MAX_SEA_LANES];
static PlagueCivCache plague_civ_cache[MAX_CIVS];
static PlagueCityCache plague_city_cache[MAX_CITIES];
static int plague_lane_exposure[MAX_SEA_LANES];
static int city_cursor, pair_cursor;
static int lane_cache_key, lane_cache_valid, lane_cache_count;
static int plague_cache_key, plague_cache_valid, plague_cache_active;
static int last_update_ms, last_update_items;
static int copy_city_cached, copy_city_stale, copy_city_fallback;
static int copy_pair_cached, copy_pair_stale, copy_pair_fallback;
static int copy_plague_cached, copy_plague_stale, copy_plague_fallback;
static int tracked_city_key, tracked_pair_key;
static int tracked_city_dirty, tracked_pair_dirty;
static const char *last_lane_source = "none";

static void track_city_key(int key, int city_limit) {
    if (tracked_city_key == key) return;
    tracked_city_key = key;
    tracked_city_dirty = clamp(city_limit, 0, MAX_CITIES);
}

static void track_pair_key(int key, int total_pairs) {
    if (tracked_pair_key == key) return;
    tracked_pair_key = key;
    tracked_pair_dirty = clamp(total_pairs, 0, MAX_CIVS * MAX_CIVS);
}

static int diplomacy_pair_relevant(int a, int b) {
    if (a < 0 || b < 0 || a >= civ_count || b >= civ_count || a >= MAX_CIVS || b >= MAX_CIVS) return 0;
    if (a == b) return civs[a].alive;
    return civs[a].alive && civs[b].alive;
}

static int diplomacy_relevant_pair_count(int civ_limit) {
    int a, b, count = 0;
    for (a = 0; a < civ_limit; a++) for (b = 0; b < civ_limit; b++)
        if (diplomacy_pair_relevant(a, b)) count++;
    return count;
}

static void copy_relation(SnapshotDiplomacyRelation *dst, DiplomacyRelation rel, int a, int b) {
    DiplomacyRelationBreakdown breakdown = diplomacy_relation_breakdown(a, b);
    int i;
    dst->state = rel.state; dst->relation_score = rel.relation_score;
    dst->border_tension = rel.border_tension; dst->trade_fit = rel.trade_fit;
    dst->resource_conflict = rel.resource_conflict; dst->truce_years_left = rel.truce_years_left;
    dst->truce_initial_years = rel.truce_initial_years; dst->border_length = rel.border_length;
    dst->natural_barrier = rel.natural_barrier; dst->years_known = rel.years_known;
    dst->vassal_years = rel.vassal_years; dst->easing_years = rel.easing_years;
    dst->contact_kind = rel.contact_kind; dst->years_distant_known = rel.years_distant_known;
    dst->overlord = rel.overlord; dst->vassal = rel.vassal;
    dst->last_war_winner = rel.last_war_winner; dst->last_war_loser = rel.last_war_loser;
    dst->last_war_result = rel.last_war_result;
    dst->state_years = diplomacy_stability_state_years(a, b);
    dst->candidate_state = diplomacy_stability_candidate_state(a, b);
    dst->candidate_years = diplomacy_stability_candidate_years(a, b);
    dst->yearly_delta_x100 = breakdown.yearly_delta_x100;
    for (i = 0; i < DIP_REL_FACTOR_SLOTS; i++) {
        dst->relation_factor_ids[i] = breakdown.factor_ids[i];
        dst->relation_factor_delta_x100[i] = breakdown.factor_delta_x100[i];
        dst->relation_factor_values[i] = breakdown.factor_values[i];
    }
}

static void copy_war(SnapshotWar *dst, ActiveWar war) {
    dst->active = war.active; dst->attacker = war.attacker; dst->defender = war.defender;
    dst->soldiers_a = war.soldiers_a; dst->soldiers_b = war.soldiers_b;
    dst->casualties_a = war.casualties_a; dst->casualties_b = war.casualties_b;
    dst->support_casualties_a = war.support_casualties_a;
    dst->support_casualties_b = war.support_casualties_b;
    dst->temporary_soldiers_a = war.temporary_soldiers_a;
    dst->temporary_soldiers_b = war.temporary_soldiers_b;
    dst->alliance_reinforcements_a = alliance_military_support_for_war(&war, 1);
    dst->alliance_reinforcements_b = alliance_military_support_for_war(&war, 0);
    dst->wins_a = war.wins_a; dst->wins_b = war.wins_b; dst->years = war.years;
}

static void refresh_city(int city_id, int key) {
    CitySummaryCache *entry;
    if (city_id < 0 || city_id >= MAX_CITIES) return;
    entry = &city_cache[city_id];
    memset(entry, 0, sizeof(*entry));
    entry->key = key;
    if (city_id >= city_count || !cities[city_id].alive) {
        entry->valid = 1;
        return;
    }
    entry->region = summarize_city_region(city_id);
    entry->population = population_city_summary(city_id);
    entry->valid = 1;
}

static void refresh_pair(int a, int b, int key) {
    DiplomacyPairCache *entry;
    if (a < 0 || b < 0 || a >= MAX_CIVS || b >= MAX_CIVS) return;
    entry = &pair_cache[a][b];
    memset(entry, 0, sizeof(*entry));
    entry->key = key;
    if (a >= civ_count || b >= civ_count) return;
    copy_relation(&entry->relation, diplomacy_relation(a, b), a, b);
    if (entry->relation.state == DIPLOMACY_WAR) {
        copy_war(&entry->war, war_state_between(a, b));
        if (entry->war.active) {
            entry->front_flags = war_front_flags(a, b);
            entry->peace_pressure = war_peace_pressure_between(a, b);
        }
    }
    entry->valid = 1;
}

static void refresh_lanes(int key) {
    const SeaLane *lanes;
    int count, i;
    lanes = sea_lanes_get(&count);
    lane_cache_count = clamp(count, 0, MAX_SEA_LANES);
    for (i = 0; i < lane_cache_count; i++) {
        SnapshotSeaLane *dst = &lane_cache[i];
        const SeaLane *src = &lanes[i];
        dst->active = src->active; dst->type = src->type;
        dst->from_node = src->from_node; dst->to_node = src->to_node;
        dst->from_region = src->from_region; dst->to_region = src->to_region;
        dst->from_city = src->from_city; dst->to_city = src->to_city;
        dst->from_port = src->from_port; dst->to_port = src->to_port;
        dst->from_sea_entry = src->from_sea_entry; dst->to_sea_entry = src->to_sea_entry;
        dst->point_count = clamp(src->point_count, 0, MAX_SEA_LANE_POINTS);
        memcpy(dst->points, src->points, (size_t)dst->point_count * sizeof(dst->points[0]));
        dst->exposure = src->exposure;
    }
    lane_cache_key = key;
    lane_cache_valid = 1;
}

static void refresh_plague(int key) {
    const SeaLane *lanes;
    int lane_count, i;
    memset(plague_civ_cache, 0, sizeof(plague_civ_cache));
    memset(plague_city_cache, 0, sizeof(plague_city_cache));
    memset(plague_lane_exposure, 0, sizeof(plague_lane_exposure));
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

void render_snapshot_cache_reset(void) {
    memset(city_cache, 0, sizeof(city_cache));
    memset(pair_cache, 0, sizeof(pair_cache));
    memset(lane_cache, 0, sizeof(lane_cache));
    memset(plague_civ_cache, 0, sizeof(plague_civ_cache));
    memset(plague_city_cache, 0, sizeof(plague_city_cache));
    memset(plague_lane_exposure, 0, sizeof(plague_lane_exposure));
    city_cursor = pair_cursor = 0;
    lane_cache_key = plague_cache_key = 0;
    lane_cache_valid = plague_cache_valid = lane_cache_count = plague_cache_active = 0;
    last_update_ms = last_update_items = 0;
    copy_city_cached = copy_city_stale = copy_city_fallback = 0;
    copy_pair_cached = copy_pair_stale = copy_pair_fallback = 0;
    copy_plague_cached = copy_plague_stale = copy_plague_fallback = 0;
    tracked_city_key = tracked_pair_key = tracked_city_dirty = tracked_pair_dirty = 0;
    last_lane_source = "none";
}

void render_snapshot_cache_begin_snapshot_copy(void) {
    copy_city_cached = copy_city_stale = copy_city_fallback = 0;
    copy_pair_cached = copy_pair_stale = copy_pair_fallback = 0;
    copy_plague_cached = copy_plague_stale = copy_plague_fallback = 0;
    last_lane_source = "none";
}

void render_snapshot_cache_update_budgeted(int city_budget, int pair_budget,
                                           int refresh_lane, int refresh_plague_cache) {
    DWORD start = GetTickCount();
    int city_key = render_snapshot_cities_revision_key();
    int pair_key = render_snapshot_diplomacy_revision_key();
    int lane_key = render_snapshot_lanes_revision_key();
    int plague_key = render_snapshot_plague_revision_key(lane_key);
    int civ_limit = clamp(civ_count, 0, MAX_CIVS);
    int city_limit = clamp(city_count, 0, MAX_CITIES);
    int scanned = 0, updated = 0, total_pairs = civ_limit * civ_limit;
    track_city_key(city_key, city_limit);
    track_pair_key(pair_key, diplomacy_relevant_pair_count(civ_limit));
    if (city_budget < 1) city_budget = 1;
    while (scanned < MAX_CITIES && updated < city_budget) {
        int id = (city_cursor + scanned) % MAX_CITIES;
        scanned++;
        if (id < city_count && (!city_cache[id].valid || city_cache[id].key != city_key)) {
            refresh_city(id, city_key);
            if (tracked_city_dirty > 0) tracked_city_dirty--;
            updated++;
        }
    }
    city_cursor = (city_cursor + scanned) % MAX_CITIES;
    scanned = 0;
    if (pair_budget < 1) pair_budget = 1;
    while (total_pairs > 0 && scanned < total_pairs && updated < city_budget + pair_budget) {
        int id = (pair_cursor + scanned) % total_pairs;
        int a = id / civ_limit, b = id % civ_limit;
        scanned++;
        if (diplomacy_pair_relevant(a, b) &&
            (!pair_cache[a][b].valid || pair_cache[a][b].key != pair_key)) {
            refresh_pair(a, b, pair_key);
            if (tracked_pair_dirty > 0) tracked_pair_dirty--;
            updated++;
        }
    }
    if (total_pairs > 0) pair_cursor = (pair_cursor + scanned) % total_pairs;
    if (world_generated && refresh_lane && (!lane_cache_valid || lane_cache_key != lane_key)) {
        refresh_lanes(lane_key);
        updated++;
    }
    if (world_generated && refresh_plague_cache && (!plague_cache_valid || plague_cache_key != plague_key)) {
        refresh_plague(plague_key);
        updated++;
    }
    last_update_ms = (int)(GetTickCount() - start);
    last_update_items = updated;
}

void render_snapshot_cache_update_all(void) {
    DWORD start = GetTickCount();
    int city_key = render_snapshot_cities_revision_key();
    int pair_key = render_snapshot_diplomacy_revision_key();
    int lane_key = render_snapshot_lanes_revision_key();
    int plague_key = render_snapshot_plague_revision_key(lane_key);
    int city_limit = clamp(city_count, 0, MAX_CITIES);
    int civ_limit = clamp(civ_count, 0, MAX_CIVS);
    int i, a, b, updated = 0;
    for (i = 0; i < city_limit; i++) {
        refresh_city(i, city_key);
        updated++;
    }
    for (a = 0; a < civ_limit; a++) {
        for (b = 0; b < civ_limit; b++) {
            if (diplomacy_pair_relevant(a, b)) {
                refresh_pair(a, b, pair_key);
                updated++;
            }
        }
    }
    if (world_generated) {
        refresh_lanes(lane_key);
        updated++;
        refresh_plague(plague_key);
        updated++;
    } else {
        lane_cache_valid = 0;
        lane_cache_count = 0;
        plague_cache_valid = 0;
        plague_cache_active = 0;
    }
    city_cursor = 0;
    pair_cursor = 0;
    tracked_city_key = city_key;
    tracked_pair_key = pair_key;
    tracked_city_dirty = 0;
    tracked_pair_dirty = 0;
    last_update_ms = (int)(GetTickCount() - start);
    last_update_items = updated;
}

static int city_dirty_count(int key) {
    track_city_key(key, clamp(city_count, 0, MAX_CITIES));
    return tracked_city_dirty;
}

static int pair_dirty_count(int key) {
    int civ_limit = clamp(civ_count, 0, MAX_CIVS);
    track_pair_key(key, diplomacy_relevant_pair_count(civ_limit));
    return tracked_pair_dirty;
}

int render_snapshot_cache_dirty_count(void) {
    int lane_key = render_snapshot_lanes_revision_key();
    int plague_key = render_snapshot_plague_revision_key(lane_key);
    int count = city_dirty_count(render_snapshot_cities_revision_key()) +
                pair_dirty_count(render_snapshot_diplomacy_revision_key());
    if (world_generated && (!lane_cache_valid || lane_cache_key != lane_key)) count++;
    if (world_generated && (!plague_cache_valid || plague_cache_key != plague_key)) count++;
    return count;
}

int render_snapshot_cache_city_ready(int key) {
    return city_dirty_count(key) <= 0;
}

int render_snapshot_cache_diplomacy_ready(int key) {
    return pair_dirty_count(key) <= 0;
}

int render_snapshot_cache_city_summary(int city_id, int key, RegionSummary *region,
                                       PopulationSummary *population) {
    CitySummaryCache *entry;
    if (city_id < 0 || city_id >= city_count || city_id >= MAX_CITIES) return 0;
    entry = &city_cache[city_id];
    if (!entry->valid || entry->key != key) { copy_city_stale++; return 0; }
    if (region) *region = entry->region;
    if (population) *population = entry->population;
    copy_city_cached++;
    return 1;
}

int render_snapshot_cache_diplomacy_pair(int a, int b, int key,
                                         SnapshotDiplomacyRelation *relation,
                                         SnapshotWar *war, int *front_flags,
                                         int *peace_pressure) {
    DiplomacyPairCache *entry;
    if (a < 0 || b < 0 || a >= civ_count || b >= civ_count || a >= MAX_CIVS || b >= MAX_CIVS) return 0;
    if (tracked_pair_key != key) track_pair_key(key, diplomacy_relevant_pair_count(clamp(civ_count, 0, MAX_CIVS)));
    if (!diplomacy_pair_relevant(a, b)) {
        if (relation) memset(relation, 0, sizeof(*relation));
        if (war) memset(war, 0, sizeof(*war));
        if (front_flags) *front_flags = 0;
        if (peace_pressure) *peace_pressure = 0;
        copy_pair_cached++;
        return 1;
    }
    entry = &pair_cache[a][b];
    if (!entry->valid || entry->key != key) {
        copy_pair_stale++;
        refresh_pair(a, b, key);
        if (tracked_pair_dirty > 0) tracked_pair_dirty--;
    }
    if (relation) *relation = entry->relation;
    if (war) *war = entry->war;
    if (front_flags) *front_flags = entry->front_flags;
    if (peace_pressure) *peace_pressure = entry->peace_pressure;
    copy_pair_cached++;
    return 1;
}

int render_snapshot_cache_lanes(int key, const SnapshotSeaLane **lanes, int *count) {
    if (!lane_cache_valid || lane_cache_key != key) { last_lane_source = "stale"; return 0; }
    if (lanes) *lanes = lane_cache;
    if (count) *count = lane_cache_count;
    last_lane_source = "cached";
    return 1;
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

void render_snapshot_cache_note_city_fallback(void) { copy_city_fallback++; }
void render_snapshot_cache_note_diplomacy_fallback(void) { copy_pair_fallback++; }
void render_snapshot_cache_note_plague_fallback(void) { copy_plague_fallback++; }

const char *render_snapshot_cache_city_summary_debug(void) {
    static char text[128];
    snprintf(text, sizeof(text), "cached %d / stale %d / fallback %d / dirty %d / update %d in %d ms",
             copy_city_cached, copy_city_stale, copy_city_fallback,
             city_dirty_count(render_snapshot_cities_revision_key()), last_update_items, last_update_ms);
    return text;
}

const char *render_snapshot_cache_diplomacy_debug(void) {
    static char text[128];
    snprintf(text, sizeof(text), "cached %d / stale %d / fallback %d / dirty %d",
             copy_pair_cached, copy_pair_stale, copy_pair_fallback,
             pair_dirty_count(render_snapshot_diplomacy_revision_key()));
    return text;
}

const char *render_snapshot_cache_plague_debug(void) {
    static char text[128];
    int lane_key = render_snapshot_lanes_revision_key();
    int dirty = !plague_cache_valid || plague_cache_key != render_snapshot_plague_revision_key(lane_key);
    snprintf(text, sizeof(text), "cached %d / stale %d / fallback %d / dirty %d / active %d",
             copy_plague_cached, copy_plague_stale, copy_plague_fallback, dirty, plague_cache_active);
    return text;
}

const char *render_snapshot_cache_lane_debug(void) {
    static char text[96];
    int key = render_snapshot_lanes_revision_key();
    snprintf(text, sizeof(text), "%s / dirty %d / lanes %d",
             last_lane_source, (!lane_cache_valid || lane_cache_key != key) ? 1 : 0, lane_cache_count);
    return text;
}
