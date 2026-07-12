#ifndef WORLD_SIM_RENDER_PARTIAL_UI_H
#define WORLD_SIM_RENDER_PARTIAL_UI_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core/render_snapshot.h"

int render_partial_ui_can_paint(RECT client, RECT paint,
                                const RenderSnapshot *snapshot);
void render_partial_ui_draw(HDC target, RECT client, RECT paint);
int render_partial_ui_map_data_dirty(void);
int render_partial_ui_stale_indicator_needed(void);
void render_partial_ui_draw_stale_indicator(HDC target, RECT client);

#endif
