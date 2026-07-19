#include "render/map_label_placement_pool.h"

#include <stdlib.h>
#include <string.h>

#define MAP_LABEL_PLACEMENT_POOL_CAPACITY 16

typedef struct {
    unsigned char *data;
    size_t allocated_bytes;
    size_t item_size;
    unsigned int key;
    unsigned int use_stamp;
    int count;
    int valid;
} PlacementPoolEntry;

static PlacementPoolEntry entries[MAP_LABEL_PLACEMENT_POOL_CAPACITY];
static unsigned int use_sequence;
static MapLabelPlacementPoolStats stats;

static void refresh_stats(void) {
    int i;
    stats.ready_entries = 0;
    stats.persistent_bytes = 0;
    for (i = 0; i < MAP_LABEL_PLACEMENT_POOL_CAPACITY; i++) {
        stats.ready_entries += entries[i].valid != 0;
        stats.persistent_bytes += entries[i].allocated_bytes;
    }
}

static void touch(PlacementPoolEntry *entry) {
    use_sequence++;
    if (!use_sequence) use_sequence++;
    entry->use_stamp = use_sequence;
}

static PlacementPoolEntry *find_entry(unsigned int key, size_t item_size) {
    int i;
    for (i = 0; i < MAP_LABEL_PLACEMENT_POOL_CAPACITY; i++) {
        if (entries[i].valid && entries[i].key == key &&
            entries[i].item_size == item_size) return &entries[i];
    }
    return NULL;
}

static PlacementPoolEntry *store_target(void) {
    PlacementPoolEntry *target = &entries[0];
    int i;
    for (i = 0; i < MAP_LABEL_PLACEMENT_POOL_CAPACITY; i++) {
        if (!entries[i].valid) return &entries[i];
    }
    for (i = 1; i < MAP_LABEL_PLACEMENT_POOL_CAPACITY; i++) {
        if (entries[i].use_stamp < target->use_stamp) target = &entries[i];
    }
    stats.evictions++;
    return target;
}

int map_label_placement_pool_load(unsigned int key, void *items,
                                  size_t item_size, int capacity, int *count) {
    PlacementPoolEntry *entry = find_entry(key, item_size);
    if (!entry || !items || !count || capacity < entry->count) {
        stats.misses++;
        return 0;
    }
    if (entry->count > 0) memcpy(items, entry->data, item_size * (size_t)entry->count);
    *count = entry->count;
    touch(entry);
    stats.hits++;
    return 1;
}

int map_label_placement_pool_store(unsigned int key, const void *items,
                                   size_t item_size, int count) {
    PlacementPoolEntry *entry;
    size_t required;
    unsigned char *replacement;
    if (!item_size || count < 0 || (count > 0 && !items)) return 0;
    entry = find_entry(key, item_size);
    if (!entry) entry = store_target();
    required = item_size * (size_t)count;
    if (required > entry->allocated_bytes) {
        replacement = (unsigned char *)realloc(entry->data, required);
        if (!replacement) return 0;
        entry->data = replacement;
        entry->allocated_bytes = required;
    }
    if (required > 0) memcpy(entry->data, items, required);
    entry->item_size = item_size;
    entry->key = key;
    entry->count = count;
    entry->valid = 1;
    touch(entry);
    stats.stores++;
    refresh_stats();
    return 1;
}

void map_label_placement_pool_invalidate(void) {
    int i;
    for (i = 0; i < MAP_LABEL_PLACEMENT_POOL_CAPACITY; i++) entries[i].valid = 0;
    use_sequence = 0;
    refresh_stats();
}

void map_label_placement_pool_reset_debug(void) {
    refresh_stats();
    stats.hits = stats.misses = stats.stores = stats.evictions = 0;
}

const MapLabelPlacementPoolStats *map_label_placement_pool_stats(void) {
    refresh_stats();
    return &stats;
}
