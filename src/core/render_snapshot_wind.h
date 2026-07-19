#ifndef WORLD_SIM_RENDER_SNAPSHOT_WIND_H
#define WORLD_SIM_RENDER_SNAPSHOT_WIND_H

#include "core/render_snapshot_wind_types.h"

int render_snapshot_wind_copy(SnapshotWindField *out, int revision_key);
const SnapshotWindSample *render_snapshot_wind_samples(const SnapshotWindField *field,
                                                       int lod, int *count);

#endif
