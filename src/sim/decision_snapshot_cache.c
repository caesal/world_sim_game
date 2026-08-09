#include "sim/decision_snapshot_cache.h"

#include "core/game_state.h"
#include "sim/territory_integrity.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int valid;
    int uid;
    DecisionSnapshot snapshot;
    char main_intent[32];
    char expansion_reason[128];
    char war_reason[128];
} DecisionSnapshotCacheEntry;

typedef struct {
    DecisionSnapshotCacheEntry entries[MAX_CIVS];
    int target_ids[MAX_CIVS];
    int target_uids[MAX_CIVS];
    int expected_count;
    int built_count;
    int cursor;
    int year;
    int month;
    uint64_t revision;
} DecisionSnapshotGeneration;

static DecisionSnapshotGeneration generations[2];
static volatile LONG published_index;
static int building_index = 1;
static int building_active;
static uint64_t published_revision_counter;
static unsigned char dirty_notices[MAX_CIVS];
static int dirty_notice_all;
static int dirty_notice_count;
static int64_t qpc_frequency;
static int64_t last_slice_us;
static int last_slice_count;
static uint64_t process_total_calculation_count;
static int restart_count;
static int cancellation_count;
static int uid_mismatch_count;

static int current_published_index(void) {
    return (int)InterlockedCompareExchange(&published_index, 0, 0);
}

static uint64_t next_revision(uint64_t current) {
    current++;
    return current == 0 ? UINT64_C(1) : current;
}

static int64_t elapsed_us(LARGE_INTEGER start) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (qpc_frequency <= 0) {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        qpc_frequency = frequency.QuadPart > 0 ? frequency.QuadPart : 1;
    }
    return (now.QuadPart - start.QuadPart) * INT64_C(1000000) / qpc_frequency;
}

static void bind_entry_strings(DecisionSnapshotCacheEntry *entry) {
    entry->snapshot.main_intent = entry->main_intent;
    entry->snapshot.expansion_reason = entry->expansion_reason;
    entry->snapshot.war_reason = entry->war_reason;
}

static void store_entry(DecisionSnapshotGeneration *generation, int civ_id, int uid,
                        const DecisionSnapshot *snapshot) {
    DecisionSnapshotCacheEntry *entry = &generation->entries[civ_id];
    memset(entry, 0, sizeof(*entry));
    entry->snapshot = *snapshot;
    entry->snapshot.published_revision = generation->revision;
    snprintf(entry->main_intent, sizeof(entry->main_intent), "%s",
             snapshot->main_intent ? snapshot->main_intent : "");
    snprintf(entry->expansion_reason, sizeof(entry->expansion_reason), "%s",
             snapshot->expansion_reason ? snapshot->expansion_reason : "");
    snprintf(entry->war_reason, sizeof(entry->war_reason), "%s",
             snapshot->war_reason ? snapshot->war_reason : "");
    bind_entry_strings(entry);
    entry->uid = uid;
    entry->valid = 1;
}

static void clear_dirty_notices(void) {
    memset(dirty_notices, 0, sizeof(dirty_notices));
    dirty_notice_all = 0;
    dirty_notice_count = 0;
}

static int pending_dirty_count(void) {
    return dirty_notice_count;
}

static void capture_building_generation(void) {
    DecisionSnapshotGeneration *generation;
    int i;

    building_index = 1 - current_published_index();
    generation = &generations[building_index];
    memset(generation, 0, sizeof(*generation));
    generation->year = year;
    generation->month = month;
    generation->revision = next_revision(published_revision_counter);
    for (i = 0; i < civ_count && i < MAX_CIVS; i++) {
        if (!civs[i].alive) continue;
        generation->target_ids[generation->expected_count] = i;
        generation->target_uids[generation->expected_count] = civs[i].uid;
        generation->expected_count++;
    }
    building_active = 1;
    clear_dirty_notices();
}

static int generation_identities_match(const DecisionSnapshotGeneration *generation) {
    int target = 0;
    int i;
    for (i = 0; i < civ_count && i < MAX_CIVS; i++) {
        if (!civs[i].alive) continue;
        if (target >= generation->expected_count ||
            generation->target_ids[target] != i ||
            generation->target_uids[target] != civs[i].uid) return 0;
        target++;
    }
    return target == generation->expected_count;
}

static void restart_building_generation(void) {
    restart_count++;
    capture_building_generation();
}

static void publish_building_generation(void) {
    DecisionSnapshotGeneration *generation = &generations[building_index];
    published_revision_counter = generation->revision;
    InterlockedExchange(&published_index, (LONG)building_index);
    building_active = 0;
    clear_dirty_notices();
}

void decision_snapshot_cache_reset(void) {
    uint64_t next = next_revision(published_revision_counter);
    memset(generations, 0, sizeof(generations));
    clear_dirty_notices();
    published_revision_counter = next;
    generations[0].revision = next;
    InterlockedExchange(&published_index, 0);
    building_index = 1;
    building_active = 0;
    last_slice_us = 0;
    last_slice_count = 0;
    restart_count = 0;
    cancellation_count = 0;
    uid_mismatch_count = 0;
}

void decision_snapshot_cache_begin_generation(void) {
    if (building_active) cancellation_count++;
    capture_building_generation();
}

int decision_snapshot_cache_service_slice(void) {
    LARGE_INTEGER start;
    int integrity_batch = 0;
    int updated = 0;
    int complete = 0;

    QueryPerformanceCounter(&start);
    if (!building_active) {
        if (pending_dirty_count() > 0) capture_building_generation();
        else {
            last_slice_count = 0;
            last_slice_us = elapsed_us(start);
            return 1;
        }
    }
    if (generations[building_index].cursor < generations[building_index].expected_count) {
        territory_integrity_read_batch_begin();
        integrity_batch = 1;
    }
    for (;;) {
        DecisionSnapshotGeneration *generation = &generations[building_index];
        int civ_id;
        int uid;

        if (generation->cursor >= generation->expected_count) {
            if (!generation_identities_match(generation)) {
                uid_mismatch_count++;
                restart_building_generation();
                continue;
            }
            publish_building_generation();
            complete = 1;
            break;
        }
        if (updated >= DECISION_SNAPSHOT_CACHE_SLICE_MAX_CIVS ||
            (updated > 0 && elapsed_us(start) >= DECISION_SNAPSHOT_CACHE_SLICE_MAX_US)) break;
        civ_id = generation->target_ids[generation->cursor];
        uid = generation->target_uids[generation->cursor];
        if (civ_id < 0 || civ_id >= civ_count || civ_id >= MAX_CIVS ||
            !civs[civ_id].alive || civs[civ_id].uid != uid) {
            uid_mismatch_count++;
            restart_building_generation();
            continue;
        }
        {
            DecisionSnapshot snapshot;
            decision_snapshot_for_civ(civ_id, &snapshot);
            updated++;
            process_total_calculation_count++;
            store_entry(generation, civ_id, uid, &snapshot);
            generation->cursor++;
            generation->built_count++;
        }
    }
    if (integrity_batch) territory_integrity_read_batch_end();
    last_slice_count = updated;
    last_slice_us = elapsed_us(start);
    return complete;
}

int decision_snapshot_cache_seed_complete(void) {
    decision_snapshot_cache_begin_generation();
    while (!decision_snapshot_cache_service_slice()) {}
    return 1;
}

void decision_snapshot_cache_cancel_building(void) {
    if (!building_active) return;
    building_active = 0;
    cancellation_count++;
}

void decision_snapshot_cache_mark_dirty(int civ_id) {
    if (civ_id < 0 || civ_id >= MAX_CIVS) return;
    if (!dirty_notice_all && !dirty_notices[civ_id] &&
        civ_id < civ_count && civs[civ_id].alive) dirty_notice_count++;
    dirty_notices[civ_id] = 1;
    decision_snapshot_cache_cancel_building();
}

void decision_snapshot_cache_mark_all_dirty(void) {
    int i;
    dirty_notice_count = 0;
    for (i = 0; i < civ_count && i < MAX_CIVS; i++) {
        if (civs[i].alive) dirty_notice_count++;
    }
    dirty_notice_all = 1;
    memset(dirty_notices, 0, sizeof(dirty_notices));
    decision_snapshot_cache_cancel_building();
}

int decision_snapshot_cached(int civ_id, DecisionSnapshot *out) {
    const DecisionSnapshotCacheEntry *entry;
    int index;
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (civ_id < 0 || civ_id >= civ_count || civ_id >= MAX_CIVS || !civs[civ_id].alive) return 0;
    index = current_published_index();
    entry = &generations[index].entries[civ_id];
    if (!entry->valid || entry->uid != civs[civ_id].uid) return 0;
    *out = entry->snapshot;
    return 1;
}

uint64_t decision_snapshot_cache_published_revision(void) {
    return generations[current_published_index()].revision;
}

int decision_snapshot_cache_valid_count(void) {
    int index = current_published_index();
    return generations[index].built_count;
}

int decision_snapshot_cache_dirty_count(void) {
    if (building_active) {
        DecisionSnapshotGeneration *generation = &generations[building_index];
        return max(0, generation->expected_count - generation->built_count);
    }
    return pending_dirty_count();
}

int decision_snapshot_cache_last_update_ms(void) {
    int64_t rounded = (last_slice_us + 999) / 1000;
    return rounded > INT_MAX ? INT_MAX : (int)rounded;
}

int decision_snapshot_cache_last_update_count(void) { return last_slice_count; }
int64_t decision_snapshot_cache_last_update_us(void) { return last_slice_us; }
uint64_t decision_snapshot_cache_total_calculation_count(void) {
    return process_total_calculation_count;
}

void decision_snapshot_cache_get_diagnostics(DecisionSnapshotCacheDiagnostics *out) {
    const DecisionSnapshotGeneration *published;
    if (!out) return;
    memset(out, 0, sizeof(*out));
    published = &generations[current_published_index()];
    out->published_revision = published->revision;
    out->published_year = published->year;
    out->published_month = published->month;
    out->published_expected_count = published->expected_count;
    out->published_built_count = published->built_count;
    out->published_count = decision_snapshot_cache_valid_count();
    out->building_active = building_active;
    if (building_active) {
        const DecisionSnapshotGeneration *building = &generations[building_index];
        out->building_revision = building->revision;
        out->building_year = building->year;
        out->building_month = building->month;
        out->building_expected_count = building->expected_count;
        out->building_built_count = building->built_count;
    }
    out->dirty_count = decision_snapshot_cache_dirty_count();
    out->last_slice_count = last_slice_count;
    out->last_slice_us = last_slice_us;
    out->total_calculation_count = process_total_calculation_count;
    out->restart_count = restart_count;
    out->cancellation_count = cancellation_count;
    out->uid_mismatch_count = uid_mismatch_count;
}
