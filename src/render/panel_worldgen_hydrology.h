#ifndef WORLD_SIM_PANEL_WORLDGEN_HYDROLOGY_H
#define WORLD_SIM_PANEL_WORLDGEN_HYDROLOGY_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "ui/ui_worldgen_control_state.h"
#include "ui/ui_worldgen_panel_layout.h"

void panel_worldgen_hydrology_draw(
    HDC hdc, const UiWorldgenPanelLayout *layout,
    const UiWorldgenEffectiveConfig *config,
    const UiWorldgenControlState *state,
    int map_width, int map_height, HFONT body_font);

#endif
