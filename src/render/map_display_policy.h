#ifndef WORLD_SIM_MAP_DISPLAY_POLICY_H
#define WORLD_SIM_MAP_DISPLAY_POLICY_H

#include "core/render_snapshot.h"
#include "render/map_ownership_surface.h"
#include <windows.h>

#define MAP_COUNTRY_FILL_ALPHA 136
#define MAP_ALLIANCE_INDEPENDENT_FILL_ALPHA 136
#define MAP_ALLIANCE_MEMBER_FILL_ALPHA 176

typedef struct {
    int active;
    COLORREF color;
    int alpha;
} MapDisplayFillPolicy;

typedef enum {
    MAP_PHYSICAL_BASE_OVERVIEW = 0,
    MAP_PHYSICAL_BASE_GEOGRAPHY,
    MAP_PHYSICAL_BASE_CLIMATE,
    MAP_PHYSICAL_BASE_COUNT
} MapPhysicalBaseFamily;

typedef enum {
    MAP_LABEL_FAMILY_ORDINARY = 0,
    MAP_LABEL_FAMILY_ALLIANCE,
    MAP_LABEL_FAMILY_REGIONS
} MapLabelFamily;

int map_display_policy_requires_fill_layer(int mode);
int map_display_policy_shows_wind(int mode);
MapPhysicalBaseFamily map_display_policy_physical_family(int mode);
MapLabelFamily map_display_policy_label_family(int mode);
COLORREF map_display_policy_snapshot_physical_color(const SnapshotTile *tile,
                                                     MapPhysicalBaseFamily family);
COLORREF map_display_policy_snapshot_smoothed_physical_color(
    const RenderSnapshot *snapshot, const SnapshotTile *tile,
    MapPhysicalBaseFamily family);
COLORREF map_display_policy_snapshot_nearby_land_color(
    const RenderSnapshot *snapshot, const SnapshotTile *tile,
    MapPhysicalBaseFamily family);
COLORREF map_display_policy_snapshot_nearby_ocean_color(
    const RenderSnapshot *snapshot, const SnapshotTile *tile,
    MapPhysicalBaseFamily family);
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
