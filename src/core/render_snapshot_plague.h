#ifndef WORLD_SIM_RENDER_SNAPSHOT_PLAGUE_H
#define WORLD_SIM_RENDER_SNAPSHOT_PLAGUE_H

#include "core/render_snapshot.h"

int render_snapshot_copy_plague(RenderSnapshot *snapshot, int key);
int render_snapshot_plague_effective_probability(int bucket);
int render_snapshot_plague_pending_probabilities_valid(void);

#endif
