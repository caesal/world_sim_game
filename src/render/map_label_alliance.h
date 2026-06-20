#ifndef WORLD_SIM_MAP_LABEL_ALLIANCE_H
#define WORLD_SIM_MAP_LABEL_ALLIANCE_H

#include "core/render_snapshot.h"
#include "render/map_label_style.h"

int map_label_alliance_source_id(int alliance_id);
int map_label_alliance_selected(const RenderSnapshot *snapshot, int source_id, int selected_civ_id);
void map_label_alliance_apply_style(MapLabelStyle *style, int source_id, int selected);
int map_label_alliance_accumulate(const RenderSnapshot *snapshot, int owner, int x, int y, int weight,
                                  long *alliance_sx, long *alliance_sy, int *alliance_weight,
                                  long *country_sx, long *country_sy, int *country_weight);

#endif
