#include "render/render_ocean_decoration_cache.h"

enum { OCEAN_DECORATION_CACHE_CAPACITY = 2 };

typedef struct {
    LayerCache cache;
    unsigned int use_stamp;
} OceanCacheEntry;

static OceanCacheEntry entries[OCEAN_DECORATION_CACHE_CAPACITY];
static unsigned int use_sequence;
static OceanDecorationCacheStats stats;

static void touch(OceanCacheEntry *entry) {
    use_sequence++;
    if (!use_sequence) use_sequence++;
    entry->use_stamp = use_sequence;
}

LayerCache *ocean_decoration_cache_find(RECT client, MapLayout layout,
                                        unsigned int key) {
    int i;
    for (i = 0; i < OCEAN_DECORATION_CACHE_CAPACITY; i++) {
        if (render_layer_cache_matches(&entries[i].cache, client, layout, key, 0)) {
            stats.hits++;
            touch(&entries[i]);
            return &entries[i].cache;
        }
    }
    stats.misses++;
    return NULL;
}

static OceanCacheEntry *replacement_entry(void) {
    OceanCacheEntry *target = &entries[0];
    int i;
    for (i = 0; i < OCEAN_DECORATION_CACHE_CAPACITY; i++) {
        if (!entries[i].cache.valid) return &entries[i];
        if (entries[i].use_stamp < target->use_stamp) target = &entries[i];
    }
    stats.evictions++;
    return target;
}

LayerCache *ocean_decoration_cache_prepare(HDC hdc, RECT client,
                                           MapLayout layout) {
    OceanCacheEntry *target = replacement_entry();
    if (!render_layer_cache_ensure(hdc, &target->cache, client, layout,
                                   side_panel_w, 0)) return NULL;
    touch(target);
    return &target->cache;
}

void ocean_decoration_cache_invalidate(void) {
    int i;
    for (i = 0; i < OCEAN_DECORATION_CACHE_CAPACITY; i++) {
        entries[i].cache.valid = 0;
        entries[i].use_stamp = 0;
    }
    use_sequence = 0;
}

void ocean_decoration_cache_reset_debug(void) {
    stats.hits = 0;
    stats.misses = 0;
    stats.evictions = 0;
}

const OceanDecorationCacheStats *ocean_decoration_cache_stats(void) {
    int i;
    stats.persistent_bitmaps = 0;
    stats.persistent_dcs = 0;
    stats.persistent_bitmap_bytes = 0;
    for (i = 0; i < OCEAN_DECORATION_CACHE_CAPACITY; i++) {
        const LayerCache *cache = &entries[i].cache;
        if (cache->bitmap) {
            stats.persistent_bitmaps++;
            stats.persistent_bitmap_bytes +=
                (unsigned long long)cache->width * (unsigned long long)cache->height * 4ull;
        }
        if (cache->dc) stats.persistent_dcs++;
    }
    return &stats;
}
