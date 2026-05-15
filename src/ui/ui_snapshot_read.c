#include "ui/ui_snapshot_read.h"

#include "core/render_snapshot.h"

int ui_snapshot_tile_owner(int x, int y) {
    const RenderSnapshot *snapshot;
    const SnapshotTile *tile;
    int owner;
    if (x < 0 || y < 0) return -1;
    snapshot = render_snapshot_acquire();
    tile = render_snapshot_tile_at(snapshot, x, y);
    owner = tile ? tile->owner : -1;
    render_snapshot_release(snapshot);
    return owner;
}

int ui_snapshot_civ_alive(int civ_id) {
    const RenderSnapshot *snapshot = render_snapshot_acquire();
    int alive = snapshot && civ_id >= 0 && civ_id < snapshot->civ_count && snapshot->civs[civ_id].alive;
    render_snapshot_release(snapshot);
    return alive;
}

unsigned int ui_snapshot_civ_color(int civ_id) {
    const RenderSnapshot *snapshot = render_snapshot_acquire();
    unsigned int color = 0;
    if (snapshot && civ_id >= 0 && civ_id < snapshot->civ_count) color = snapshot->civs[civ_id].color;
    render_snapshot_release(snapshot);
    return color;
}
