#ifndef WORLD_SIM_UI_WORLDGEN_VIEW_H
#define WORLD_SIM_UI_WORLDGEN_VIEW_H

#include <windows.h>

#include "ui/ui_worldgen_panel_layout.h"
#include "ui/ui_worldgen_layout.h"

void ui_worldgen_view_build(RECT client, int panel_width,
                            UiWorldgenPanelLayout *layout);
void ui_worldgen_view_build_legacy_layout(
    const UiWorldgenLegacyLayout *legacy, WorldgenLayout *layout);

#endif
