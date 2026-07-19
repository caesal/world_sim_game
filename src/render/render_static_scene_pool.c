#include "render/render_static_scene_pool.h"

#include <string.h>

typedef struct {
    LayerCache cache;
    unsigned int boundary_key;
    unsigned int use_stamp;
    int current;
    int safe;
    int full;
} ScenePoolEntry;

static ScenePoolEntry entries[RENDER_STATIC_SCENE_POOL_CAPACITY];
static unsigned int use_sequence;
static RenderStaticScenePoolStats stats;

static void view_from_entry(ScenePoolEntry *entry, RenderStaticScenePoolView *view) {
    if (!view) return;
    view->cache = entry ? &entry->cache : NULL;
    view->boundary_key = entry ? entry->boundary_key : 0;
    view->current = entry ? entry->current : 0;
    view->safe = entry ? entry->safe : 0;
    view->full = entry ? entry->full : 0;
}

static void touch_entry(ScenePoolEntry *entry) {
    use_sequence++;
    if (!use_sequence) use_sequence++;
    entry->use_stamp = use_sequence;
}

static ScenePoolEntry *find_exact_entry(RECT client, MapLayout layout,
                                        unsigned int key, int display) {
    int i;
    for (i = 0; i < RENDER_STATIC_SCENE_POOL_CAPACITY; i++) {
        if (render_layer_cache_matches(&entries[i].cache, client, layout, key, display))
            return &entries[i];
    }
    return NULL;
}

int render_static_scene_pool_find_exact(RECT client, MapLayout layout,
                                        unsigned int key, int display,
                                        RenderStaticScenePoolView *view) {
    ScenePoolEntry *entry = find_exact_entry(client, layout, key, display);
    if (!entry) {
        stats.misses++;
        view_from_entry(NULL, view);
        return 0;
    }
    stats.hits++;
    touch_entry(entry);
    view_from_entry(entry, view);
    return 1;
}

int render_static_scene_pool_find_presentable(RECT client, int display,
                                              RenderStaticScenePoolView *view) {
    ScenePoolEntry *best = NULL;
    int i;
    for (i = 0; i < RENDER_STATIC_SCENE_POOL_CAPACITY; i++) {
        ScenePoolEntry *entry = &entries[i];
        if (!render_layer_cache_preview_presentable(&entry->cache, client, display)) continue;
        if (!best || entry->use_stamp > best->use_stamp) best = entry;
    }
    if (!best) {
        view_from_entry(NULL, view);
        return 0;
    }
    touch_entry(best);
    view_from_entry(best, view);
    return 1;
}

static ScenePoolEntry *publish_target(RECT client, MapLayout layout,
                                      unsigned int key, int display) {
    ScenePoolEntry *target = find_exact_entry(client, layout, key, display);
    int i;
    if (target) return target;
    for (i = 0; i < RENDER_STATIC_SCENE_POOL_CAPACITY; i++) {
        if (!entries[i].cache.valid) return &entries[i];
    }
    target = &entries[0];
    for (i = 1; i < RENDER_STATIC_SCENE_POOL_CAPACITY; i++) {
        if (entries[i].use_stamp < target->use_stamp) target = &entries[i];
    }
    stats.evictions++;
    return target;
}

static void refresh_memory_stats(void) {
    int i;
    stats.persistent_bitmaps = 0;
    stats.persistent_dcs = 0;
    stats.persistent_bitmap_bytes = 0;
    for (i = 0; i < RENDER_STATIC_SCENE_POOL_CAPACITY; i++) {
        const LayerCache *cache = &entries[i].cache;
        if (cache->bitmap) {
            stats.persistent_bitmaps++;
            stats.persistent_bitmap_bytes +=
                (unsigned long long)cache->width * (unsigned long long)cache->height * 4ull;
        }
        if (cache->dc) stats.persistent_dcs++;
    }
}

int render_static_scene_pool_publish(HDC hdc, RECT client, MapLayout layout,
                                     const LayerCache *source, unsigned int key,
                                     unsigned int boundary_key, int display,
                                     int current, int safe, int full,
                                     RenderStaticScenePoolView *view) {
    ScenePoolEntry *target;
    if (!hdc || !source || !source->valid || !safe) return 0;
    target = publish_target(client, layout, key, display);
    if (!render_layer_cache_ensure(hdc, &target->cache, client, layout, side_panel_w,
                                   display)) return 0;
    BitBlt(target->cache.dc, 0, 0, target->cache.width, target->cache.height,
           source->dc, 0, 0, SRCCOPY);
    target->cache.key = key;
    target->cache.valid = 1;
    target->boundary_key = boundary_key;
    target->current = current;
    target->safe = safe;
    target->full = full;
    touch_entry(target);
    stats.publishes++;
    refresh_memory_stats();
    view_from_entry(target, view);
    return 1;
}

void render_static_scene_pool_invalidate(void) {
    int i;
    for (i = 0; i < RENDER_STATIC_SCENE_POOL_CAPACITY; i++) {
        entries[i].cache.valid = 0;
        entries[i].boundary_key = 0;
        entries[i].current = entries[i].safe = entries[i].full = 0;
        entries[i].use_stamp = 0;
    }
    use_sequence = 0;
}

void render_static_scene_pool_reset_debug(void) {
    int bitmaps;
    int dcs;
    unsigned long long bytes;
    refresh_memory_stats();
    bitmaps = stats.persistent_bitmaps;
    dcs = stats.persistent_dcs;
    bytes = stats.persistent_bitmap_bytes;
    memset(&stats, 0, sizeof(stats));
    stats.persistent_bitmaps = bitmaps;
    stats.persistent_dcs = dcs;
    stats.persistent_bitmap_bytes = bytes;
}

const RenderStaticScenePoolStats *render_static_scene_pool_stats(void) {
    refresh_memory_stats();
    return &stats;
}
