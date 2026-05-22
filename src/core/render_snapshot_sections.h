#ifndef WORLD_SIM_RENDER_SNAPSHOT_SECTIONS_H
#define WORLD_SIM_RENDER_SNAPSHOT_SECTIONS_H

#include "core/render_snapshot.h"

void render_snapshot_copy_skipped_sections(RenderSnapshot *dst, const RenderSnapshot *src);
void render_snapshot_seed_from_front(RenderSnapshot *dst, const RenderSnapshot *src);

#endif
