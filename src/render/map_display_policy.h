#ifndef WORLD_SIM_MAP_DISPLAY_POLICY_H
#define WORLD_SIM_MAP_DISPLAY_POLICY_H

#include "core/render_snapshot.h"
#include "render/map_ownership_surface.h"
#include <windows.h>

typedef struct {
    int active;
    COLORREF color;
    int alpha;
} MapDisplayFillPolicy;

int map_display_policy_requires_fill_layer(int mode);
COLORREF map_display_policy_snapshot_base_color(const SnapshotTile *tile, int mode);
int map_display_policy_snapshot_effective_owner(const RenderSnapshot *snapshot,
                                                const SnapshotTile *tile,
                                                MapDisplayOwnerSource *out_source);
int map_display_policy_snapshot_fill(const RenderSnapshot *snapshot, const SnapshotTile *tile,
                                     int mode, MapDisplayFillPolicy *out_fill);
int map_display_policy_snapshot_owner_fill(const RenderSnapshot *snapshot, int owner,
                                           int mode, MapDisplayFillPolicy *out_fill);
COLORREF map_display_policy_snapshot_tile_color(const RenderSnapshot *snapshot,
                                                const SnapshotTile *tile, int mode);
int map_display_policy_live_effective_owner(int x, int y, MapDisplayOwnerSource *out_source);
int map_display_policy_live_fill(int x, int y, int mode, MapDisplayFillPolicy *out_fill);
int map_display_policy_live_owner_fill(int owner, int mode, MapDisplayFillPolicy *out_fill);
COLORREF map_display_policy_live_tile_color(int x, int y, int mode);

#endif
