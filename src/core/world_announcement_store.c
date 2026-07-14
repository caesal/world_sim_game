#include "core/world_announcement_store.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdlib.h>

#define WORLD_ANNOUNCEMENT_STORE_CAP 128

static WorldAnnouncementEvent *events;
static int event_count;
static int event_capacity;
static int event_start;
static int total_entries;
static CRITICAL_SECTION store_lock;
static INIT_ONCE store_lock_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK init_lock_once(PINIT_ONCE once, PVOID param, PVOID *context) {
    (void)once;
    (void)param;
    (void)context;
    InitializeCriticalSection(&store_lock);
    return TRUE;
}

static void init_lock(void) {
    InitOnceExecuteOnce(&store_lock_once, init_lock_once, NULL, NULL);
}

static int ensure_capacity_locked(void) {
    if (event_capacity == WORLD_ANNOUNCEMENT_STORE_CAP) return 1;
    events = (WorldAnnouncementEvent *)calloc(WORLD_ANNOUNCEMENT_STORE_CAP, sizeof(*events));
    if (!events) return 0;
    event_capacity = WORLD_ANNOUNCEMENT_STORE_CAP;
    return 1;
}

void world_announcement_store_clear(void) {
    init_lock();
    EnterCriticalSection(&store_lock);
    event_count = 0;
    event_start = 0;
    total_entries = 0;
    LeaveCriticalSection(&store_lock);
}

int world_announcement_store_append(const WorldAnnouncementEvent *event) {
    int i;
    int index;
    int ok = 0;
    if (!event || event->event_id <= 0) return 0;
    init_lock();
    EnterCriticalSection(&store_lock);
    for (i = event_count - 1; i >= 0; i--) {
        int same_plague_source;
        index = (event_start + i) % WORLD_ANNOUNCEMENT_STORE_CAP;
        same_plague_source = event->plague.valid && events[index].plague.valid &&
            event->plague.stable_event_id != 0 &&
            events[index].plague.stable_event_id == event->plague.stable_event_id;
        if (events[index].event_id != event->event_id && !same_plague_source) continue;
        events[index] = *event;
        ok = 1;
        break;
    }
    if (!ok && ensure_capacity_locked()) {
        if (event_count < WORLD_ANNOUNCEMENT_STORE_CAP) {
            index = (event_start + event_count++) % WORLD_ANNOUNCEMENT_STORE_CAP;
        } else {
            index = event_start;
            event_start = (event_start + 1) % WORLD_ANNOUNCEMENT_STORE_CAP;
        }
        events[index] = *event;
        total_entries++;
        ok = 1;
    }
    LeaveCriticalSection(&store_lock);
    return ok;
}

int world_announcement_store_count(void) {
    int count;
    init_lock();
    EnterCriticalSection(&store_lock);
    count = event_count;
    LeaveCriticalSection(&store_lock);
    return count;
}

int world_announcement_store_total_entries(void) {
    int total;
    init_lock();
    EnterCriticalSection(&store_lock);
    total = total_entries;
    LeaveCriticalSection(&store_lock);
    return total;
}

int world_announcement_store_copy_newest(WorldAnnouncementEvent *out, int max_events) {
    int count;
    int start;
    int i;
    if (!out || max_events <= 0) return 0;
    init_lock();
    EnterCriticalSection(&store_lock);
    count = event_count < max_events ? event_count : max_events;
    start = event_count - count;
    for (i = 0; i < count; i++) {
        out[i] = events[(event_start + start + i) % WORLD_ANNOUNCEMENT_STORE_CAP];
    }
    LeaveCriticalSection(&store_lock);
    return count;
}

int world_announcement_store_copy_newest_stream(WorldAnnouncementStreamEntry *out,
                                                int max_events) {
    int count;
    int start;
    int i;
    if (!out || max_events <= 0) return 0;
    init_lock();
    EnterCriticalSection(&store_lock);
    count = event_count < max_events ? event_count : max_events;
    start = event_count - count;
    for (i = 0; i < count; i++) {
        const WorldAnnouncementEvent *event =
            &events[(event_start + start + i) % WORLD_ANNOUNCEMENT_STORE_CAP];
        out[i].event_id = event->event_id;
        out[i].event_type = event->event_type;
        out[i].priority = event->priority;
        out[i].year = event->year;
        out[i].month = event->month;
    }
    LeaveCriticalSection(&store_lock);
    return count;
}

int world_announcement_store_get_by_event_id(int event_id, WorldAnnouncementEvent *out) {
    int i;
    int ok = 0;
    if (!out || event_id <= 0) return 0;
    init_lock();
    EnterCriticalSection(&store_lock);
    for (i = event_count - 1; i >= 0; i--) {
        int index = (event_start + i) % WORLD_ANNOUNCEMENT_STORE_CAP;
        if (events[index].event_id != event_id) continue;
        *out = events[index];
        ok = 1;
        break;
    }
    LeaveCriticalSection(&store_lock);
    return ok;
}
