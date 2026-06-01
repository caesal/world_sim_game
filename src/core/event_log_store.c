#include "core/event_log_store.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int event_id;
    EventLogEntry entry;
} StoredEvent;

static StoredEvent *events;
static int event_count;
static int event_capacity;
static int next_event_id = 1;
static CRITICAL_SECTION store_lock;
static INIT_ONCE store_lock_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK event_store_lock_init_once(PINIT_ONCE once, PVOID param, PVOID *context) {
    (void)once; (void)param; (void)context;
    InitializeCriticalSection(&store_lock);
    return TRUE;
}

static void event_store_lock_init(void) {
    InitOnceExecuteOnce(&store_lock_once, event_store_lock_init_once, NULL, NULL);
}

static int ensure_capacity_locked(int needed) {
    int new_capacity;
    StoredEvent *grown;
    if (needed <= event_capacity) return 1;
    new_capacity = event_capacity > 0 ? event_capacity * 2 : 512;
    while (new_capacity < needed) new_capacity *= 2;
    grown = (StoredEvent *)realloc(events, sizeof(StoredEvent) * (size_t)new_capacity);
    if (!grown) return 0;
    events = grown;
    event_capacity = new_capacity;
    return 1;
}

void event_log_store_clear(void) {
    event_store_lock_init();
    EnterCriticalSection(&store_lock);
    event_count = 0;
    next_event_id = 1;
    LeaveCriticalSection(&store_lock);
}

int event_log_store_append(const EventLogEntry *entry) {
    int id = 0;
    event_store_lock_init();
    if (!entry) return 0;
    EnterCriticalSection(&store_lock);
    if (ensure_capacity_locked(event_count + 1)) {
        id = next_event_id++;
        events[event_count].event_id = id;
        events[event_count].entry = *entry;
        event_count++;
    }
    LeaveCriticalSection(&store_lock);
    return id;
}

int event_log_store_increment_repeat(int event_id) {
    int i;
    int ok = 0;
    event_store_lock_init();
    if (event_id <= 0) return 0;
    EnterCriticalSection(&store_lock);
    if (event_id <= event_count && events[event_id - 1].event_id == event_id) {
        events[event_id - 1].entry.repeat_count++;
        LeaveCriticalSection(&store_lock);
        return 1;
    }
    for (i = event_count - 1; i >= 0; i--) {
        if (events[i].event_id != event_id) continue;
        events[i].entry.repeat_count++;
        ok = 1;
        break;
    }
    LeaveCriticalSection(&store_lock);
    return ok;
}

int event_log_store_count(void) {
    int count;
    event_store_lock_init();
    EnterCriticalSection(&store_lock);
    count = event_count;
    LeaveCriticalSection(&store_lock);
    return count;
}

int event_log_store_get_newest(int index, EventLogEntry *out) {
    int pos;
    event_store_lock_init();
    if (!out || index < 0) return 0;
    EnterCriticalSection(&store_lock);
    pos = event_count - 1 - index;
    if (pos >= 0 && pos < event_count) *out = events[pos].entry;
    LeaveCriticalSection(&store_lock);
    return pos >= 0 && pos < event_count;
}

int event_log_store_get_oldest(int index, EventLogEntry *out) {
    int ok;
    event_store_lock_init();
    if (!out || index < 0) return 0;
    EnterCriticalSection(&store_lock);
    ok = index < event_count;
    if (ok) *out = events[index].entry;
    LeaveCriticalSection(&store_lock);
    return ok;
}

int event_log_store_get_by_id(int event_id, EventLogEntry *out) {
    int i;
    int ok = 0;
    event_store_lock_init();
    if (!out || event_id <= 0) return 0;
    EnterCriticalSection(&store_lock);
    if (event_id <= event_count && events[event_id - 1].event_id == event_id) {
        *out = events[event_id - 1].entry;
        LeaveCriticalSection(&store_lock);
        return 1;
    }
    for (i = event_count - 1; i >= 0; i--) {
        if (events[i].event_id != event_id) continue;
        *out = events[i].entry;
        ok = 1;
        break;
    }
    LeaveCriticalSection(&store_lock);
    return ok;
}
