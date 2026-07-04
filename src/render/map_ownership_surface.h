#ifndef WORLD_SIM_MAP_OWNERSHIP_SURFACE_H
#define WORLD_SIM_MAP_OWNERSHIP_SURFACE_H

#include "core/render_snapshot.h"

typedef enum {
    MAP_DISPLAY_OWNER_NONE = 0,
    MAP_DISPLAY_OWNER_TILE = 1,
    MAP_DISPLAY_OWNER_REGION = 2,
    MAP_DISPLAY_OWNER_CITY = 3,
    MAP_DISPLAY_OWNER_REGION_CITY = 4
} MapDisplayOwnerSource;

typedef struct {
    int width;
    int height;
    const short *owner;
    const unsigned char *source;
} MapOwnershipSurfaceView;

int map_ownership_surface_snapshot_owner(const RenderSnapshot *snapshot, int x, int y,
                                         MapDisplayOwnerSource *out_source);
int map_ownership_surface_live_owner(int x, int y, MapDisplayOwnerSource *out_source);
int map_ownership_surface_snapshot_view(const RenderSnapshot *snapshot, MapOwnershipSurfaceView *out_view);
int map_ownership_surface_live_view(MapOwnershipSurfaceView *out_view);
int map_ownership_surface_snapshot_revision(const RenderSnapshot *snapshot);
int map_ownership_surface_live_revision(void);
void map_ownership_surface_invalidate_live(void);
int map_ownership_surface_snapshot_area_for_civ(const RenderSnapshot *snapshot, int civ_id);

#endif
