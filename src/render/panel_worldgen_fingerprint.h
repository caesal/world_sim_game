#ifndef WORLD_SIM_PANEL_WORLDGEN_FINGERPRINT_H
#define WORLD_SIM_PANEL_WORLDGEN_FINGERPRINT_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "ui/ui_worldgen_panel_layout.h"

void panel_worldgen_fingerprint_draw(
    HDC hdc, const UiWorldgenPanelLayout *layout, HFONT body_font);

#endif
