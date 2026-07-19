#ifndef WORLD_SIM_RENDER_SNAPSHOT_RIVER_H
#define WORLD_SIM_RENDER_SNAPSHOT_RIVER_H

#include <stddef.h>

#include "core/render_snapshot_river_types.h"

int render_snapshot_river_copy(SnapshotRiverField *out, int revision_key,
                               int map_w, int map_h);
int render_snapshot_river_clone(SnapshotRiverField *out,
                                const SnapshotRiverField *source);
int render_snapshot_river_reserve(SnapshotRiverField *field, int capacity);
void render_snapshot_river_release(SnapshotRiverField *field);
size_t render_snapshot_river_field_retained_bytes(
    const SnapshotRiverField *field);

#endif
