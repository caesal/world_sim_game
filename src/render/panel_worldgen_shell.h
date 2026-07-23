#ifndef WORLD_SIM_PANEL_WORLDGEN_SHELL_H
#define WORLD_SIM_PANEL_WORLDGEN_SHELL_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "ui/ui_worldgen_control_state.h"
#include "ui/ui_worldgen_panel_layout.h"

void panel_worldgen_shell_draw(HDC hdc,
                               const UiWorldgenPanelLayout *layout,
                               const UiWorldgenControlState *state,
                               HFONT title_font, HFONT body_font);
void panel_worldgen_shell_draw_tooltip(
    HDC hdc, RECT client, const UiWorldgenControlState *state);

#endif
