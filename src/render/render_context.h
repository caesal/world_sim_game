#ifndef WORLD_SIM_RENDER_CONTEXT_H
#define WORLD_SIM_RENDER_CONTEXT_H

#include "core/render_snapshot.h"

typedef struct {
    const RenderSnapshot *snapshot;
} RenderContext;

void render_context_begin(const RenderSnapshot *snapshot);
void render_context_end(void);
const RenderSnapshot *render_context_snapshot(void);

#endif
