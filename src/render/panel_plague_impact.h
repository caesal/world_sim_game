#ifndef WORLD_SIM_PANEL_PLAGUE_IMPACT_H
#define WORLD_SIM_PANEL_PLAGUE_IMPACT_H

#include "core/render_snapshot.h"
#include "ui/ui_plague_panel_layout.h"

void plague_panel_impact_draw(HDC hdc, const UiPlagueImpactLayout *layout,
                              const RenderSnapshot *snapshot);

#endif
