#ifndef WORLD_SIM_PANEL_PLAGUE_LIVE_H
#define WORLD_SIM_PANEL_PLAGUE_LIVE_H

#include "core/render_snapshot.h"
#include "ui/ui_widgets.h"

typedef enum {
    PLAGUE_PANEL_LIVE_EMPTY,
    PLAGUE_PANEL_LIVE_COMPLETED,
    PLAGUE_PANEL_LIVE_ACTIVE
} PlaguePanelLiveMode;

PlaguePanelLiveMode plague_panel_live_mode(const RenderSnapshot *snapshot);
void plague_panel_live_draw(HDC hdc, UiCursor *cursor,
                            const RenderSnapshot *snapshot);

#endif
