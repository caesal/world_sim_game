#ifndef WORLD_SIM_PANEL_PLAGUE_HISTORY_H
#define WORLD_SIM_PANEL_PLAGUE_HISTORY_H

#include "core/render_snapshot.h"
#include "ui/ui_plague_panel_layout.h"

void plague_panel_history_draw(HDC hdc, const UiPlagueHistoryLayout *layout,
                               const RenderSnapshot *snapshot);

#endif
