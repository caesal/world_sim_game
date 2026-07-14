#ifndef WORLD_SIM_PANEL_PLAGUE_PROBABILITY_H
#define WORLD_SIM_PANEL_PLAGUE_PROBABILITY_H

#include "core/render_snapshot.h"
#include "ui/ui_plague_probability.h"

void plague_panel_probability_draw(HDC hdc,
    const UiPlagueProbabilityLayout *layout,
    const RenderSnapshot *snapshot);

#endif
