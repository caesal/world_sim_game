#ifndef WORLD_SIM_MAP_LABEL_PLACEMENT_POOL_H
#define WORLD_SIM_MAP_LABEL_PLACEMENT_POOL_H

#include <stddef.h>

typedef struct {
    int hits;
    int misses;
    int stores;
    int evictions;
    int ready_entries;
    size_t persistent_bytes;
} MapLabelPlacementPoolStats;

int map_label_placement_pool_load(unsigned int key, void *items,
                                  size_t item_size, int capacity, int *count);
int map_label_placement_pool_store(unsigned int key, const void *items,
                                   size_t item_size, int count);
void map_label_placement_pool_invalidate(void);
void map_label_placement_pool_reset_debug(void);
const MapLabelPlacementPoolStats *map_label_placement_pool_stats(void);

#endif
