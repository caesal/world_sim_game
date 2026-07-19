#ifndef WORLD_SIM_RENDER_OCEAN_DECORATION_ITEMS_H
#define WORLD_SIM_RENDER_OCEAN_DECORATION_ITEMS_H

#include "core/render_snapshot.h"

typedef struct {
    unsigned char type;
    unsigned char variant;
    unsigned char interior;
    unsigned char opacity;
    short x;
    short y;
    short size;
} OceanDecorationItem;

typedef struct {
    const OceanDecorationItem *exterior_items;
    const OceanDecorationItem *interior_items;
    int exterior_count;
    int interior_count;
    int item_rebuilds;
    int motif_overlap_count;
    int motif_spacing_violation_count;
    int same_type_spacing_ok;
    int exterior_spacing_ok;
    unsigned int key;
    unsigned int hash;
    unsigned int motif_mask;
} OceanDecorationItemSet;

const OceanDecorationItemSet *render_ocean_decoration_items_ensure(
    const RenderSnapshot *snapshot);
const OceanDecorationItemSet *render_ocean_decoration_items_ready(
    const RenderSnapshot *snapshot);
int render_ocean_decoration_item_clearance_tiles(
    const RenderSnapshot *snapshot, const OceanDecorationItem *item);
void render_ocean_decoration_items_reset(void);

#endif
