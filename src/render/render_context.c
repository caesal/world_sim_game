#include "render/render_context.h"

static RenderContext current_context;

void render_context_begin(const RenderSnapshot *snapshot) {
    current_context.snapshot = snapshot;
}

void render_context_end(void) {
    current_context.snapshot = NULL;
}

const RenderSnapshot *render_context_snapshot(void) {
    return current_context.snapshot;
}
