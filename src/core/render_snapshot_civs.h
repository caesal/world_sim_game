#ifndef WORLD_SIM_RENDER_SNAPSHOT_CIVS_H
#define WORLD_SIM_RENDER_SNAPSHOT_CIVS_H

#include "core/render_snapshot.h"

void render_snapshot_copy_civs_locked(RenderSnapshot *snapshot);
int render_snapshot_civ_decision_cached_count(void);
int render_snapshot_civ_decision_stale_count(void);
int render_snapshot_civ_decision_fallback_count(void);

#endif
