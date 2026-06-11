#ifndef WORLD_SIM_PANEL_COUNTRY_POPULATION_CARDS_H
#define WORLD_SIM_PANEL_COUNTRY_POPULATION_CARDS_H

#include "core/sim_types.h"
#include "render/render_common.h"
#include "ui/ui_widgets.h"

void draw_population_structure_cards(HDC hdc, UiCursor *cursor,
                                     PopulationSummary summary,
                                     int current_soldiers);

#endif
