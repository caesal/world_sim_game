#ifndef WORLD_SIM_RENDER_SNAPSHOT_KEYS_H
#define WORLD_SIM_RENDER_SNAPSHOT_KEYS_H

int render_snapshot_tile_revision_key(void);
int render_snapshot_civs_revision_key(void);
int render_snapshot_cities_revision_key(void);
int render_snapshot_regions_revision_key(void);
int render_snapshot_diplomacy_revision_key(void);
int render_snapshot_lanes_revision_key(void);
int render_snapshot_plague_revision_key(int lane_key);

#endif
