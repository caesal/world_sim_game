#ifndef WORLD_SIM_PANEL_COUNTRY_POPULATION_H
#define WORLD_SIM_PANEL_COUNTRY_POPULATION_H

#include "render/render_common.h"
#include "ui/ui_widgets.h"

int country_population_tab_height(int civ_id);
void draw_country_population_tab(HDC hdc, RECT client, UiCursor *cursor,
                                 int civ_id, HFONT body_font);

#endif
