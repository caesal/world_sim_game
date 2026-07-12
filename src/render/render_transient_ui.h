#ifndef WORLD_SIM_RENDER_TRANSIENT_UI_H
#define WORLD_SIM_RENDER_TRANSIENT_UI_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int render_transient_ui_prepare(HDC hdc, RECT client, int progress_active);
int render_transient_ui_has_visible(int progress_active);
void render_transient_ui_draw_full(HDC hdc, RECT client, int progress_active);
int render_transient_ui_can_partial(RECT client, RECT paint);
int render_transient_ui_draw_partial(HDC hdc, RECT client, RECT paint);
void render_transient_ui_reset_probe_metrics(void);
int render_transient_ui_underlay_captures(void);
int render_transient_ui_underlay_restores(void);

#endif
