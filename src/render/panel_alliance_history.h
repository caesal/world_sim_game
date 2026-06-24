#ifndef WORLD_SIM_PANEL_ALLIANCE_HISTORY_H
#define WORLD_SIM_PANEL_ALLIANCE_HISTORY_H

#include "render/panel_alliance_model.h"
#include "ui/ui_widgets.h"

int alliance_history_content_height(const RenderSnapshot *snapshot, const AlliancePanelRow *row);
void alliance_history_draw_content(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                                   const AlliancePanelRow *row);
void alliance_history_probe_sentence(char *out, int out_size, const RenderSnapshot *snapshot,
                                     const AllianceHistoryRecord *history);

#endif
