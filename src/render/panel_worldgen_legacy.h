#ifndef WORLD_SIM_PANEL_WORLDGEN_LEGACY_H
#define WORLD_SIM_PANEL_WORLDGEN_LEGACY_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "ui/ui_worldgen_panel_layout.h"

void draw_worldgen_legacy_panel(HDC hdc,
                                const UiWorldgenLegacyLayout *legacy,
                                RECT client,
                                HFONT title_font, HFONT body_font);

#endif
