#ifndef WORLD_SIM_RENDER_SNAPSHOT_EVENTS_H
#define WORLD_SIM_RENDER_SNAPSHOT_EVENTS_H

#include "core/render_snapshot.h"

void render_snapshot_copy_events_locked(RenderSnapshot *snapshot);
void render_snapshot_format_events(RenderSnapshot *snapshot);

#endif
