#include "render/render_ocean_decoration_water.h"

#include "render/render_ocean_assets.h"
#include "render/render_ocean_decoration_rules.h"
#include "world/terrain_query.h"

int ocean_decoration_water_tile(const RenderSnapshot *snapshot, int x, int y) {
    const SnapshotTile *t;
    if (!snapshot || x < 0 || y < 0 || x >= snapshot->map_w || y >= snapshot->map_h) return 0;
    t = &snapshot->tiles[y * snapshot->map_w + x];
    return t->water_depth != WATER_DEPTH_NONE || t->geography == GEO_OCEAN ||
           t->geography == GEO_BAY || t->geography == GEO_LAKE;
}

int ocean_decoration_deep_ocean_tile(const RenderSnapshot *snapshot, int x, int y) {
    const SnapshotTile *t;
    if (!snapshot || x < 0 || y < 0 || x >= snapshot->map_w || y >= snapshot->map_h) return 0;
    t = &snapshot->tiles[y * snapshot->map_w + x];
    return t->water_depth == WATER_DEPTH_DEEP && t->geography == GEO_OCEAN &&
           t->water_deep_percent >= 70;
}

static int open_ocean_tile(const RenderSnapshot *snapshot, int x, int y) {
    const SnapshotTile *t;
    if (!snapshot || x < 0 || y < 0 || x >= snapshot->map_w || y >= snapshot->map_h) return 0;
    t = &snapshot->tiles[y * snapshot->map_w + x];
    return t->geography == GEO_OCEAN &&
           (t->water_depth == WATER_DEPTH_SHALLOW || t->water_depth == WATER_DEPTH_DEEP);
}

static int motif_water_tile(const RenderSnapshot *snapshot, int x, int y, unsigned char type) {
    const OceanMotifAssetInfo *info = ocean_assets_motif_info(type);
    if (ocean_decoration_motif_water_rule(info) == OCEAN_MOTIF_WATER_SHALLOW_OR_DEEP) {
        return open_ocean_tile(snapshot, x, y);
    }
    return ocean_decoration_deep_ocean_tile(snapshot, x, y);
}

int ocean_decoration_deep_clearance(const RenderSnapshot *snapshot, int x, int y,
                                    int max_radius) {
    int r, dx, dy;
    if (!ocean_decoration_deep_ocean_tile(snapshot, x, y)) return 0;
    for (r = 1; r <= max_radius; r++) {
        for (dy = -r; dy <= r; dy++) {
            for (dx = -r; dx <= r; dx++) {
                if (dx * dx + dy * dy > r * r) continue;
                if (!ocean_decoration_deep_ocean_tile(snapshot, x + dx, y + dy)) return r - 1;
            }
        }
    }
    return max_radius;
}

int ocean_decoration_motif_clearance(const RenderSnapshot *snapshot, int x, int y,
                                     unsigned char type, int max_radius) {
    int r, dx, dy;
    if (!motif_water_tile(snapshot, x, y, type)) return 0;
    for (r = 1; r <= max_radius; r++) {
        for (dy = -r; dy <= r; dy++) {
            for (dx = -r; dx <= r; dx++) {
                if (dx * dx + dy * dy > r * r) continue;
                if (!motif_water_tile(snapshot, x + dx, y + dy, type)) return r - 1;
            }
        }
    }
    return max_radius;
}
