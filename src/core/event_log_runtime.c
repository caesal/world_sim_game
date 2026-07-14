#include "core/event_log_classify.h"
#include "core/event_log_history.h"
#include "core/event_log_store.h"
#include "core/game_state.h"
#include "core/world_announcement_store.h"

#include "sim/simulation.h"

#include <stdio.h>
#include <string.h>

char event_log[EVENT_LOG_COUNT][EVENT_LOG_LEN];
static EventLogEntry event_log_entries[EVENT_LOG_COUNT];
int event_log_count = 0;
int event_log_next = 0;
int event_log_total_entries = 0;
static int event_log_last_event_id = 0;
static EventLogEntry event_log_last_entry;

static int event_param_a_is_civ(EventLogType type) {
    return type == EVENT_TYPE_VASSAL_TRANSFERRED;
}

static void event_snapshot_civ(EventCivSnapshot *snapshot, int civ_id) {
    if (!snapshot) return;
    memset(snapshot, 0, sizeof(*snapshot));
    if (civ_id < 0 || civ_id >= civ_count) return;
    snapshot->uid = civs[civ_id].uid;
    snapshot->symbol = civs[civ_id].symbol;
    snapshot->color = civs[civ_id].color;
    snprintf(snapshot->name_en, sizeof(snapshot->name_en), "%s",
             civilization_display_name_for_language(civ_id, 0));
    snprintf(snapshot->name_zh, sizeof(snapshot->name_zh), "%s",
             civilization_display_name_for_language(civ_id, 1));
}

static void event_snapshot_plague_origin(EventCivSnapshot *snapshot,
                                         const PlagueEventPayload *plague) {
    if (!snapshot || !plague || !plague->valid) return;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->uid = plague->origin_civ_uid;
    snapshot->symbol = plague->origin_civ_symbol;
    snapshot->color = plague->origin_civ_color;
    snprintf(snapshot->name_en, sizeof(snapshot->name_en), "%s",
             plague->origin_civ_name_en);
    snprintf(snapshot->name_zh, sizeof(snapshot->name_zh), "%s",
             plague->origin_civ_name_zh);
}

static int event_plague_matches(const PlagueEventPayload *plague) {
    if (!plague || !plague->valid) return !event_log_last_entry.plague.valid;
    return event_log_last_entry.plague.valid &&
           event_log_last_entry.plague.stable_event_id == plague->stable_event_id;
}

static int event_log_push_structured_payload(
    EventLogType type, EventLogSeverity severity, int civ_id,
    int target_id, int region_id, int city_id,
    int param_a, int param_b, const char *raw_message,
    const PlagueEventPayload *plague) {
    EventLogEntry *entry;
    int previous;
    int civ_uid = plague && plague->valid ? plague->origin_civ_uid :
                  (civ_id >= 0 && civ_id < civ_count ? civs[civ_id].uid : 0);
    int target_uid = target_id >= 0 && target_id < civ_count ? civs[target_id].uid : 0;
    int param_a_uid = event_param_a_is_civ(type) && param_a >= 0 && param_a < civ_count ?
                      civs[param_a].uid : 0;
    if (!raw_message) raw_message = "";
    event_log_total_entries++;
    if (event_log_last_entry.type == type && event_log_last_entry.civ_id == civ_id &&
        event_log_last_entry.target_id == target_id && event_log_last_entry.civ_uid == civ_uid &&
        event_log_last_entry.target_uid == target_uid && event_log_last_entry.param_a_uid == param_a_uid &&
        event_log_last_entry.region_id == region_id && event_log_last_entry.city_id == city_id &&
        event_log_last_entry.param_a == param_a && event_log_last_entry.param_b == param_b &&
        strcmp(event_log_last_entry.raw_message, raw_message) == 0 &&
        event_plague_matches(plague) && event_log_last_event_id > 0) {
        previous = event_log_next - 1;
        while (previous < 0) previous += EVENT_LOG_COUNT;
        event_log_entries[previous % EVENT_LOG_COUNT].repeat_count++;
        event_log_store_increment_repeat(event_log_last_event_id);
        return event_log_last_event_id;
    }
    entry = &event_log_entries[event_log_next];
    memset(entry, 0, sizeof(*entry));
    entry->year = year;
    entry->month = month;
    entry->type = type;
    entry->severity = severity;
    entry->civ_id = civ_id;
    entry->target_id = target_id;
    entry->region_id = region_id;
    entry->city_id = city_id;
    entry->param_a = param_a;
    entry->param_b = param_b;
    entry->repeat_count = 1;
    entry->civ_uid = civ_uid;
    entry->target_uid = target_uid;
    entry->param_a_uid = param_a_uid;
    event_snapshot_civ(&entry->civ_snapshot, civ_id);
    event_snapshot_civ(&entry->target_snapshot, target_id);
    if (event_param_a_is_civ(type)) event_snapshot_civ(&entry->param_a_snapshot, param_a);
    if (plague && plague->valid) {
        entry->plague = *plague;
        event_snapshot_plague_origin(&entry->civ_snapshot, plague);
        entry->civ_uid = plague->origin_civ_uid;
    }
    snprintf(entry->raw_message, sizeof(entry->raw_message), "%s", raw_message);
    snprintf(event_log[event_log_next], EVENT_LOG_LEN, "%s", raw_message);
    event_log_last_entry = *entry;
    event_log_last_event_id = event_log_store_append(entry);
    event_log_history_store_related_id(entry, event_log_last_event_id);
    event_log_next = (event_log_next + 1) % EVENT_LOG_COUNT;
    event_log_count = event_log_store_count();
    return event_log_last_event_id;
}

int event_log_push_structured_id(EventLogType type, EventLogSeverity severity, int civ_id,
                                 int target_id, int region_id, int city_id,
                                 int param_a, int param_b, const char *raw_message) {
    return event_log_push_structured_payload(type, severity, civ_id, target_id,
                                             region_id, city_id, param_a, param_b,
                                             raw_message, NULL);
}

int event_log_push_plague_event(EventLogType type, EventLogSeverity severity,
                                const PlagueEventPayload *payload) {
    if (!payload || !payload->valid || payload->stable_event_id == 0) return 0;
    if (type != EVENT_TYPE_PLAGUE_STARTED && type != EVENT_TYPE_PLAGUE_ENDED) return 0;
    return event_log_push_structured_payload(
        type, severity, payload->origin_civ_id, -1, -1,
        payload->origin_city_id, payload->episode_id, payload->size, "", payload);
}

void event_log_push_structured(EventLogType type, EventLogSeverity severity, int civ_id,
                               int target_id, int region_id, int city_id,
                               int param_a, int param_b, const char *raw_message) {
    (void)event_log_push_structured_id(type, severity, civ_id, target_id, region_id,
                                      city_id, param_a, param_b, raw_message);
}

void event_log_push(const char *text) {
    EventLogType type;
    if (!text || !text[0]) return;
    type = event_log_type_from_text(text);
    event_log_push_structured(type, event_log_severity_from_type(type),
                              -1, -1, -1, -1, 0, 0, text);
}

void event_log_clear(void) {
    memset(event_log, 0, sizeof(event_log));
    memset(event_log_entries, 0, sizeof(event_log_entries));
    event_log_store_clear();
    event_log_history_clear();
    world_announcement_store_clear();
    event_log_count = 0;
    event_log_next = 0;
    event_log_total_entries = 0;
    event_log_last_event_id = 0;
    memset(&event_log_last_entry, 0, sizeof(event_log_last_entry));
}

EventLogType event_log_get_type(int index) {
    EventLogEntry entry;
    return event_log_get_entry(index, &entry) ? entry.type : EVENT_TYPE_GENERIC;
}

static int event_type_is_country_scoped(EventLogType type) {
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

static int event_civ_identity_matches(int civ_id, int stored_id, int stored_uid) {
    if (stored_id != civ_id || stored_uid <= 0) return 0;
    return civ_id >= 0 && civ_id < civ_count && civs[civ_id].uid == stored_uid;
}

int event_log_entry_involves_civ(const EventLogEntry *entry, int civ_id) {
    if (!entry || civ_id < 0 || civ_id >= civ_count) return 0;
    if (!event_type_is_country_scoped(entry->type)) return 0;
    if (event_civ_identity_matches(civ_id, entry->civ_id, entry->civ_uid)) return 1;
    if (event_civ_identity_matches(civ_id, entry->target_id, entry->target_uid)) return 1;
    if (event_param_a_is_civ(entry->type) &&
        event_civ_identity_matches(civ_id, entry->param_a, entry->param_a_uid)) return 1;
    return 0;
}

int event_log_get_entry(int index, EventLogEntry *out) {
    if (!out || index < 0 || index >= event_log_count) return 0;
    return event_log_store_get_newest(index, out);
}

void event_log_copy_save_state(EventLogEntry *entries, int max_entries,
                               int *count, int *next, int *total) {
    int i;
    int store_count = event_log_store_count();
    int n = clamp(min(max_entries, store_count), 0, EVENT_LOG_COUNT);
    if (entries && n > 0) {
        memset(entries, 0, sizeof(EventLogEntry) * (size_t)max_entries);
        for (i = 0; i < n; i++) {
            event_log_store_get_oldest(store_count - n + i, &entries[i]);
        }
    }
    if (count) *count = n;
    if (next) *next = n % EVENT_LOG_COUNT;
    if (total) *total = event_log_total_entries;
}

void event_log_restore_save_state(const EventLogEntry *entries, int count, int next, int total) {
    int i;
    event_log_clear();
    count = clamp(count, 0, EVENT_LOG_COUNT);
    event_log_total_entries = max(0, total);
    if (!entries) return;
    for (i = 0; i < count; i++) {
        int pos = clamp(next, 0, EVENT_LOG_COUNT - 1) - count + i;
        EventLogEntry entry;
        while (pos < 0) pos += EVENT_LOG_COUNT;
        entry = entries[pos % EVENT_LOG_COUNT];
        if (!entry.raw_message[0] && entry.type == EVENT_TYPE_GENERIC && entry.civ_uid <= 0) continue;
        event_log_entries[event_log_next] = entry;
        snprintf(event_log[event_log_next], EVENT_LOG_LEN, "%s", entry.raw_message);
        event_log_next = (event_log_next + 1) % EVENT_LOG_COUNT;
        event_log_last_event_id = event_log_store_append(&entry);
        event_log_history_store_related_id(&entry, event_log_last_event_id);
        event_log_last_entry = entry;
    }
    event_log_count = event_log_store_count();
    event_log_total_entries = max(event_log_total_entries, event_log_count);
}
