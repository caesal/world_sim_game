#ifndef WORLD_SIM_RENDER_WORLD_STATIC_PREWARM_H
#define WORLD_SIM_RENDER_WORLD_STATIC_PREWARM_H

#include <windows.h>

#include "core/render_snapshot.h"
#include "ui/ui_layout.h"

typedef struct {
    int attempts;
    int completions;
    int failures;
    int layouts_completed;
    int modes_completed;
    int last_ms;
    int requests;
    int injected_allocation_failures;
} RenderWorldStaticPrewarmStats;

int render_world_static_prewarm_layout(HDC hdc, RECT client, MapLayout layout,
                                       int zoom_percent,
                                       const RenderSnapshot *snapshot);
int render_world_static_prewarm_from_published(HWND hwnd);
const RenderWorldStaticPrewarmStats *render_world_static_prewarm_stats(void);

#endif
