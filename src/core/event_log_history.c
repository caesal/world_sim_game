#include "core/event_log_history.h"

#include <string.h>

static EventLogEntry civ_event_history[MAX_CIVS][EVENT_LOG_CIV_HISTORY_COUNT];
static int civ_event_next[MAX_CIVS];
static int civ_event_count[MAX_CIVS];

static int event_param_a_is_civ_history(EventLogType type) {
    return type == EVENT_TYPE_VASSAL_TRANSFERRED;
}

static int event_type_is_country_scoped_history(EventLogType type) {
    switch (type) {
        case EVENT_TYPE_GENERIC:
        case EVENT_TYPE_PERFORMANCE_THROTTLED:
        case EVENT_TYPE_PERFORMANCE_SLOW_CALL:
        case EVENT_TYPE_SCHEDULER_YIELD:
        case EVENT_TYPE_WORLD_GENERATION_NOTICE:
        case EVENT_TYPE_DEBUG_NOTICE:
            return 0;
        default:
            return 1;
    }
}

static int identity_matches_uid(int civ_id, int civ_uid, int stored_id, int stored_uid) {
    return civ_uid > 0 && stored_uid == civ_uid && stored_id == civ_id;
}

int event_log_entry_involves_civ_uid(const EventLogEntry *entry, int civ_id, int civ_uid) {
    if (!entry || civ_id < 0 || civ_uid <= 0) return 0;
    if (!event_type_is_country_scoped_history(entry->type)) return 0;
    if (identity_matches_uid(civ_id, civ_uid, entry->civ_id, entry->civ_uid)) return 1;
    if (identity_matches_uid(civ_id, civ_uid, entry->target_id, entry->target_uid)) return 1;
    if (event_param_a_is_civ_history(entry->type) &&
        identity_matches_uid(civ_id, civ_uid, entry->param_a, entry->param_a_uid)) return 1;
    return 0;
}

static void store_one(int civ_id, int civ_uid, const EventLogEntry *entry) {
    int pos;
    if (!entry || civ_id < 0 || civ_id >= MAX_CIVS || civ_uid <= 0) return;
    if (!event_log_entry_involves_civ_uid(entry, civ_id, civ_uid)) return;
    pos = civ_event_next[civ_id];
    civ_event_history[civ_id][pos] = *entry;
    civ_event_next[civ_id] = (pos + 1) % EVENT_LOG_CIV_HISTORY_COUNT;
    if (civ_event_count[civ_id] < EVENT_LOG_CIV_HISTORY_COUNT) civ_event_count[civ_id]++;
}

void event_log_history_store_related(const EventLogEntry *entry) {
    if (!entry || !event_type_is_country_scoped_history(entry->type)) return;
    store_one(entry->civ_id, entry->civ_uid, entry);
    if (entry->target_id != entry->civ_id || entry->target_uid != entry->civ_uid) {
        store_one(entry->target_id, entry->target_uid, entry);
    }
    if (event_param_a_is_civ_history(entry->type) &&
        (entry->param_a != entry->civ_id || entry->param_a_uid != entry->civ_uid) &&
        (entry->param_a != entry->target_id || entry->param_a_uid != entry->target_uid)) {
        store_one(entry->param_a, entry->param_a_uid, entry);
    }
}

void event_log_history_clear(void) {
    memset(civ_event_history, 0, sizeof(civ_event_history));
    memset(civ_event_next, 0, sizeof(civ_event_next));
    memset(civ_event_count, 0, sizeof(civ_event_count));
}

int event_log_recent_count_for_civ_uid(int civ_id, int civ_uid) {
    int i;
    int count = 0;
    if (civ_id < 0 || civ_id >= MAX_CIVS || civ_uid <= 0) return 0;
    for (i = 0; i < civ_event_count[civ_id]; i++) {
        if (event_log_entry_involves_civ_uid(&civ_event_history[civ_id][i], civ_id, civ_uid)) count++;
    }
    return count;
}

int event_log_recent_for_civ_uid(int civ_id, int civ_uid, int index, EventLogEntry *out) {
    int checked;
    int seen = 0;
    if (!out || civ_id < 0 || civ_id >= MAX_CIVS || civ_uid <= 0 || index < 0) return 0;
    for (checked = 0; checked < civ_event_count[civ_id]; checked++) {
        int pos = civ_event_next[civ_id] - 1 - checked;
        while (pos < 0) pos += EVENT_LOG_CIV_HISTORY_COUNT;
        if (!event_log_entry_involves_civ_uid(&civ_event_history[civ_id][pos], civ_id, civ_uid)) continue;
        if (seen++ == index) {
            *out = civ_event_history[civ_id][pos];
            return 1;
        }
    }
    return 0;
}
