#include "ui/world_announcement_queue.h"

#include "core/world_announcement_store.h"

#include <stdlib.h>
#include <string.h>

#define ANNOUNCEMENT_QUEUE_MAX 128
#define ANNOUNCEMENT_PAGE_MIN_MS 1500

typedef struct {
    WorldAnnouncementEvent event;
    int elapsed_ms;
    int page_elapsed_ms;
    int page;
    int page_count;
    int seen_count;
    unsigned long long sequence;
    unsigned char page_seen[MAX_CIVS];
} AnnouncementQueueItem;

static AnnouncementQueueItem *current;
static AnnouncementQueueItem *pending[ANNOUNCEMENT_QUEUE_MAX];
static int pending_count;
static int hovered;
static int last_snapshot_total;
static int last_event_id;
static DWORD last_tick;
static unsigned long long next_sequence;

int world_announcement_queue_duration_ms(int priority) {
    if (priority >= WORLD_ANNOUNCEMENT_CRITICAL) return 7000;
    if (priority >= WORLD_ANNOUNCEMENT_MAJOR) return 5000;
    return 3500;
}

static AnnouncementQueueItem *create_item(const WorldAnnouncementEvent *event) {
    AnnouncementQueueItem *item = (AnnouncementQueueItem *)calloc(1, sizeof(*item));
    if (!item) return NULL;
    item->event = *event;
    item->page_count = 1;
    item->sequence = next_sequence++;
    return item;
}

void world_announcement_queue_reset(void) {
    int i;
    free(current);
    current = NULL;
    for (i = 0; i < pending_count; i++) free(pending[i]);
    memset(pending, 0, sizeof(pending));
    pending_count = 0;
    hovered = 0;
    last_snapshot_total = 0;
    last_event_id = 0;
    last_tick = GetTickCount();
    next_sequence = 1;
}

static int pending_append(AnnouncementQueueItem *item) {
    if (!item || pending_count >= ANNOUNCEMENT_QUEUE_MAX) return 0;
    pending[pending_count++] = item;
    return 1;
}

static void make_current(AnnouncementQueueItem *item) {
    current = item;
    last_tick = GetTickCount();
}

static void enqueue_event(const WorldAnnouncementEvent *event) {
    AnnouncementQueueItem *item = create_item(event);
    if (!item) return;
    if (!current) {
        make_current(item);
        return;
    }
    if (event->priority >= WORLD_ANNOUNCEMENT_CRITICAL &&
        current->event.priority < WORLD_ANNOUNCEMENT_CRITICAL &&
        pending_append(current)) {
        make_current(item);
        return;
    }
    if (!pending_append(item)) free(item);
}

void world_announcement_queue_consume(const RenderSnapshot *snapshot) {
    int i;
    if (!snapshot) return;
    if (snapshot->world_announcement_total_entries < last_snapshot_total) {
        world_announcement_queue_reset();
    }
    last_snapshot_total = snapshot->world_announcement_total_entries;
    for (i = 0; i < snapshot->world_announcement_count; i++) {
        const WorldAnnouncementStreamEntry *entry = &snapshot->world_announcements[i];
        WorldAnnouncementEvent event;
        if (entry->event_id <= last_event_id) continue;
        if (world_announcement_store_get_by_event_id(entry->event_id, &event)) {
            enqueue_event(&event);
        }
        last_event_id = entry->event_id;
    }
}

static void select_next(void) {
    int i;
    int best = -1;
    int best_priority = -1;
    unsigned long long best_sequence = ~0ULL;
    free(current);
    current = NULL;
    for (i = 0; i < pending_count; i++) {
        if (pending[i]->event.priority > best_priority ||
            (pending[i]->event.priority == best_priority &&
             pending[i]->sequence < best_sequence)) {
            best = i;
            best_priority = pending[i]->event.priority;
            best_sequence = pending[i]->sequence;
        }
    }
    if (best >= 0) {
        AnnouncementQueueItem *next = pending[best];
        for (i = best; i + 1 < pending_count; i++) pending[i] = pending[i + 1];
        pending[--pending_count] = NULL;
        make_current(next);
    }
}

static int effective_duration(const AnnouncementQueueItem *item) {
    int base = world_announcement_queue_duration_ms(item->event.priority);
    int pages = max(1, item->page_count) * ANNOUNCEMENT_PAGE_MIN_MS;
    return max(base, pages);
}

static int next_unseen_page(const AnnouncementQueueItem *item) {
    int offset;
    for (offset = 1; offset <= item->page_count; offset++) {
        int page = (item->page + offset) % item->page_count;
        if (!item->page_seen[page]) return page;
    }
    return (item->page + 1) % item->page_count;
}

int world_announcement_queue_tick(DWORD now) {
    int delta;
    int page_interval;
    if (!last_tick) last_tick = now;
    delta = (int)(now - last_tick);
    last_tick = now;
    if (!current) return 0;
    if (delta < 0) delta = 0;
    if (delta > 250) delta = 250;
    if (hovered) return 1;
    current->elapsed_ms += delta;
    current->page_elapsed_ms += delta;
    page_interval = max(ANNOUNCEMENT_PAGE_MIN_MS,
                        effective_duration(current) / max(1, current->page_count));
    if (current->page_count > 1 && current->page_elapsed_ms >= page_interval) {
        current->page = next_unseen_page(current);
        current->page_elapsed_ms = 0;
    }
    if (current->elapsed_ms >= effective_duration(current) &&
        current->seen_count >= current->page_count) {
        select_next();
    }
    return 1;
}

int world_announcement_queue_active(void) { return current != NULL; }

const WorldAnnouncementEvent *world_announcement_queue_current(void) {
    return current ? &current->event : NULL;
}

int world_announcement_queue_pending_count(void) {
    return pending_count + (current ? 1 : 0);
}

int world_announcement_queue_current_page(void) { return current ? current->page : 0; }
int world_announcement_queue_page_count(void) { return current ? current->page_count : 0; }

void world_announcement_queue_set_page_count(int count) {
    if (!current) return;
    count = clamp(count, 1, MAX_CIVS);
    if (current->page_count == count) return;
    current->page_count = count;
    if (current->page >= count) current->page = 0;
    memset(current->page_seen, 0, sizeof(current->page_seen));
    current->seen_count = 0;
    current->page_elapsed_ms = 0;
}

void world_announcement_queue_mark_page_visible(void) {
    if (!current || current->page < 0 || current->page >= current->page_count) return;
    if (!current->page_seen[current->page]) {
        current->page_seen[current->page] = 1;
        current->seen_count++;
    }
}

void world_announcement_queue_previous_page(void) {
    if (!current || current->page_count <= 1) return;
    current->page = (current->page + current->page_count - 1) % current->page_count;
    current->page_elapsed_ms = 0;
}

void world_announcement_queue_next_page(void) {
    if (!current || current->page_count <= 1) return;
    current->page = (current->page + 1) % current->page_count;
    current->page_elapsed_ms = 0;
}

void world_announcement_queue_dismiss(void) {
    if (current) select_next();
}

void world_announcement_queue_set_hovered(int is_hovered) { hovered = is_hovered ? 1 : 0; }

int world_announcement_queue_progress_permille(void) {
    int duration;
    if (!current) return 0;
    duration = max(1, effective_duration(current));
    return clamp(current->elapsed_ms * 1000 / duration, 0, 1000);
}
