#include "render/render_static_map_cache.h"
#include "render/render_static_map_cache_status.h"

typedef struct {
    int reason_counts[RENDER_STATIC_MAP_REASON_COUNT];
    RenderStaticMapReason last_reason;
    int presented_current;
    int presented_boundary_safe;
    int presented_fully_current;
    int ownership_current;
    int snapshot_static_key;
    int live_static_key;
    int snapshot_fill_key;
    int snapshot_border_key;
    int live_fill_key;
    int live_border_key;
    int published_fill_key;
    int published_border_key;
    int pending_since_ms;
    int pending_ms;
    int last_latency_ms;
} RenderStaticMapCacheStatus;

static RenderStaticMapCacheStatus status;

static const char *reason_names[RENDER_STATIC_MAP_REASON_COUNT] = {
    "none", "physical", "fill", "coast", "hydro", "border"
};

static int now_ms(void) { return (int)GetTickCount(); }

static int keys_pending(void) {
    int expected_fill = status.live_fill_key ? status.live_fill_key : status.snapshot_fill_key;
    return status.snapshot_fill_key != 0 &&
           (status.published_fill_key != expected_fill ||
            status.published_border_key != status.snapshot_border_key);
}

static void refresh_pending_timer(void) {
    if (keys_pending()) {
        if (status.pending_since_ms == 0) status.pending_since_ms = now_ms();
        status.pending_ms = now_ms() - status.pending_since_ms;
    } else {
        if (status.pending_since_ms != 0) status.last_latency_ms = now_ms() - status.pending_since_ms;
        status.pending_since_ms = 0;
        status.pending_ms = 0;
    }
}

void render_static_map_cache_status_note_reason(RenderStaticMapReason reason) {
    if (reason < 0 || reason >= RENDER_STATIC_MAP_REASON_COUNT) reason = RENDER_STATIC_MAP_REASON_NONE;
    status.last_reason = reason;
    status.reason_counts[reason]++;
}

void render_static_map_cache_status_set_keys(int snapshot_static_key, int live_static_key,
                                             int snapshot_fill_key, int snapshot_border_key,
                                             int live_fill_key, int live_border_key) {
    status.snapshot_static_key = snapshot_static_key;
    status.live_static_key = live_static_key;
    status.snapshot_fill_key = snapshot_fill_key;
    status.snapshot_border_key = snapshot_border_key;
    status.live_fill_key = live_fill_key;
    status.live_border_key = live_border_key;
    refresh_pending_timer();
}

void render_static_map_cache_status_set_presented(int current, int boundary_safe,
                                                  int fully_current, int ownership_current) {
    status.presented_current = current;
    status.presented_boundary_safe = boundary_safe;
    status.presented_fully_current = fully_current;
    status.ownership_current = ownership_current;
    refresh_pending_timer();
}

void render_static_map_cache_status_note_published(int fill_key, int border_key,
                                                   int ownership_current) {
    if (ownership_current) {
        status.published_fill_key = fill_key;
        status.published_border_key = border_key;
    }
    refresh_pending_timer();
}

void render_static_map_cache_status_reset_keys(void) {
    status.snapshot_static_key = 0;
    status.live_static_key = 0;
    status.snapshot_fill_key = 0;
    status.snapshot_border_key = 0;
    status.live_fill_key = 0;
    status.live_border_key = 0;
    status.published_fill_key = 0;
    status.published_border_key = 0;
    status.pending_since_ms = 0;
    status.pending_ms = 0;
}

int render_static_map_cache_needs_work(void);

const char *render_static_map_cache_last_reason(void) {
    return reason_names[status.last_reason];
}

const char *render_static_map_cache_reason_summary(void) {
    static char text[128];
    snprintf(text, sizeof(text), "phys %d / fill %d / coast %d / hydro %d / border %d",
             status.reason_counts[RENDER_STATIC_MAP_REASON_PHYSICAL],
             status.reason_counts[RENDER_STATIC_MAP_REASON_FILL],
             status.reason_counts[RENDER_STATIC_MAP_REASON_COAST],
             status.reason_counts[RENDER_STATIC_MAP_REASON_HYDRO],
             status.reason_counts[RENDER_STATIC_MAP_REASON_BORDER]);
    return text;
}

int render_static_map_cache_presented_current(void) { return status.presented_current; }
int render_static_map_cache_presented_complete(void) { return status.presented_boundary_safe; }
int render_static_map_cache_presented_boundary_safe(void) { return status.presented_boundary_safe; }
int render_static_map_cache_presented_fully_current(void) { return status.presented_fully_current; }
int render_static_map_cache_snapshot_revision(void) { return status.snapshot_static_key; }
int render_static_map_cache_live_revision(void) { return status.live_static_key; }
int render_static_map_cache_snapshot_fill_revision(void) { return status.snapshot_fill_key; }
int render_static_map_cache_snapshot_border_revision(void) { return status.snapshot_border_key; }
int render_static_map_cache_live_fill_revision(void) { return status.live_fill_key; }
int render_static_map_cache_live_border_revision(void) { return status.live_border_key; }
int render_static_map_cache_published_fill_revision(void) { return status.published_fill_key; }
int render_static_map_cache_published_border_revision(void) { return status.published_border_key; }
int render_static_map_cache_ownership_current(void) { return status.ownership_current; }
int render_static_map_cache_political_pending_ms(void) {
    refresh_pending_timer();
    return status.pending_ms;
}
int render_static_map_cache_political_last_latency_ms(void) { return status.last_latency_ms; }
