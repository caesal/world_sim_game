#include "core/render_snapshot_wind.h"

#include "world/world_physical_state.h"

#include <string.h>

static int copy_samples(SnapshotWindSample *out, int capacity, int lod) {
    int count = world_physical_state_wind_sample_count(lod);
    int i;
    if (count < 0 || count > capacity) return -1;
    for (i = 0; i < count; i++) {
        int x;
        int y;
        int direction;
        int speed;
        if (!world_physical_state_wind_sample(lod, i, &x, &y, &direction, &speed) ||
            x < 0 || x >= world_physical_state_width() ||
            y < 0 || y >= world_physical_state_height() ||
            direction < 0 || direction >= WORLD_WIND_DIRECTION_COUNT ||
            speed < 0 || speed > 100) return -1;
        out[i].x = (uint16_t)x;
        out[i].y = (uint16_t)y;
        out[i].direction = (uint8_t)direction;
        out[i].speed = (uint8_t)speed;
    }
    return count;
}

int render_snapshot_wind_copy(SnapshotWindField *out, int revision_key) {
    int coarse;
    int medium;
    int fine;
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    out->revision = revision_key;
    if (!world_physical_state_valid()) return 1;
    coarse = copy_samples(out->coarse, SNAPSHOT_WIND_COARSE_MAX, SNAPSHOT_WIND_LOD_COARSE);
    medium = copy_samples(out->medium, SNAPSHOT_WIND_MEDIUM_MAX, SNAPSHOT_WIND_LOD_MEDIUM);
    fine = copy_samples(out->fine, SNAPSHOT_WIND_FINE_MAX, SNAPSHOT_WIND_LOD_FINE);
    if (coarse < 0 || medium < 0 || fine < 0) {
        memset(out, 0, sizeof(*out));
        return 0;
    }
    out->valid = 1;
    out->revision = revision_key;
    out->map_w = world_physical_state_width();
    out->map_h = world_physical_state_height();
    out->coarse_count = coarse;
    out->medium_count = medium;
    out->fine_count = fine;
    return 1;
}

const SnapshotWindSample *render_snapshot_wind_samples(const SnapshotWindField *field,
                                                       int lod, int *count) {
    if (count) *count = 0;
    if (!field || !field->valid) return NULL;
    if (lod == SNAPSHOT_WIND_LOD_COARSE) {
        if (count) *count = field->coarse_count;
        return field->coarse;
    }
    if (lod == SNAPSHOT_WIND_LOD_MEDIUM) {
        if (count) *count = field->medium_count;
        return field->medium;
    }
    if (lod == SNAPSHOT_WIND_LOD_FINE) {
        if (count) *count = field->fine_count;
        return field->fine;
    }
    return NULL;
}
