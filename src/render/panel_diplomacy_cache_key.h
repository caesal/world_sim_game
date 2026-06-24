#ifndef WORLD_SIM_PANEL_DIPLOMACY_CACHE_KEY_H
#define WORLD_SIM_PANEL_DIPLOMACY_CACHE_KEY_H

#include "core/render_snapshot.h"

unsigned int panel_diplomacy_rows_cache_key(unsigned int key, const RenderSnapshot *snapshot,
                                            int civ_id, int all_relations);
unsigned int panel_diplomacy_rows_cache_key_for_view(unsigned int key, const RenderSnapshot *snapshot,
                                                     int civ_id, int all_relations, int include_war_live);

#endif
