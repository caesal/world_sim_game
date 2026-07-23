#ifndef WORLD_SIM_PANEL_WORLDGEN_PHYSICAL_H
#define WORLD_SIM_PANEL_WORLDGEN_PHYSICAL_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "ui/ui_worldgen_control_state.h"
#include "ui/ui_worldgen_panel_layout.h"

void panel_worldgen_physical_draw(
    HDC hdc, const UiWorldgenPanelLayout *layout,
    const UiWorldgenEffectiveConfig *config,
    const UiWorldgenControlState *state, HFONT body_font);

#endif
