#ifndef WORLD_SIM_SNAPSHOT_UI_H
#define WORLD_SIM_SNAPSHOT_UI_H

#include "core/render_snapshot.h"

const RenderSnapshot *snapshot_ui_current(void);
const SnapshotCiv *snapshot_ui_civ(int civ_id);
const SnapshotCity *snapshot_ui_city(int city_id);
const SnapshotRegion *snapshot_ui_region(int region_id);
const SnapshotTile *snapshot_ui_tile(int x, int y);
int snapshot_ui_world_generated(void);
int snapshot_ui_civ_count(void);
int snapshot_ui_city_count(void);
int snapshot_ui_region_count(void);
int snapshot_ui_civ_visible(int civ_id, int include_fallen);
int snapshot_ui_tile_owner(int x, int y);
int snapshot_ui_city_at(int x, int y);
int snapshot_ui_region_city_for_tile(int x, int y);
int snapshot_ui_province_count(int civ_id);
const char *snapshot_ui_civ_name(int civ_id);
const char *snapshot_ui_capital_name(int civ_id);
const char *snapshot_ui_region_name(int region_id);

#endif
