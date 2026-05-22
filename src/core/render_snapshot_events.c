#include "core/render_snapshot_events.h"

#include <string.h>

void render_snapshot_copy_events_locked(RenderSnapshot *snapshot) {
    int max_events = min(event_log_count, RENDER_SNAPSHOT_EVENT_COUNT);
    int i;
    snapshot->event_count = max_events;
    snapshot->event_total_entries = event_log_total_entries;
    for (i = 0; i < max_events; i++) {
        SnapshotEvent *dst = &snapshot->events[i];
        memset(dst, 0, sizeof(*dst));
        event_log_get_entry(i, &dst->entry);
        dst->type = dst->entry.type;
    }
    memset(snapshot->civ_recent_event_count, 0, sizeof(snapshot->civ_recent_event_count));
    for (i = 0; i < snapshot->civ_count && i < MAX_CIVS; i++) {
        int j;
        int uid = snapshot->civs[i].uid;
        int count = min(event_log_recent_count_for_civ_uid(i, uid), EVENT_LOG_CIV_HISTORY_COUNT);
        snapshot->civ_recent_event_count[i] = count;
        for (j = 0; j < count; j++) {
            SnapshotEvent *dst = &snapshot->civ_recent_events[i][j];
            memset(dst, 0, sizeof(*dst));
            if (!event_log_recent_for_civ_uid(i, uid, j, &dst->entry)) continue;
            dst->type = dst->entry.type;
        }
    }
}

void render_snapshot_format_events(RenderSnapshot *snapshot) {
    int i;
    int j;
    if (!snapshot) return;
    for (i = 0; i < snapshot->event_count; i++) {
        SnapshotEvent *dst = &snapshot->events[i];
        event_log_format_entry_data(&dst->entry, 0, dst->text_en, sizeof(dst->text_en));
        event_log_format_entry_data(&dst->entry, 1, dst->text_zh, sizeof(dst->text_zh));
    }
    for (i = 0; i < snapshot->civ_count && i < MAX_CIVS; i++) {
        int count = render_snapshot_civ_recent_event_count(snapshot, i);
        for (j = 0; j < count; j++) {
            SnapshotEvent *dst = &snapshot->civ_recent_events[i][j];
            event_log_format_entry_data(&dst->entry, 0, dst->text_en, sizeof(dst->text_en));
            event_log_format_entry_data(&dst->entry, 1, dst->text_zh, sizeof(dst->text_zh));
        }
    }
}
