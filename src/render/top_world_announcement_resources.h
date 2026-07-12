#ifndef WORLD_SIM_TOP_WORLD_ANNOUNCEMENT_RESOURCES_H
#define WORLD_SIM_TOP_WORLD_ANNOUNCEMENT_RESOURCES_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "render/icons.h"

void top_world_announcement_resources_ensure(void);
void top_world_announcement_resources_prewarm(HDC target);
void top_world_announcement_draw_cached_icon(HDC target, IconId icon,
                                             RECT rect, COLORREF fallback);
int top_world_announcement_text_width(HDC target, const char *text);
void top_world_announcement_note_draw_phases(int layout_ms, int content_ms,
                                             int composite_ms);
void top_world_announcement_reset_phase_metrics(void);
HFONT top_world_announcement_header_font(void);
HFONT top_world_announcement_body_font(void);
HFONT top_world_announcement_metadata_font(void);
HBRUSH top_world_announcement_border_brush(void);

#endif
