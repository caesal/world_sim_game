#ifndef WORLD_SIM_PANEL_COUNTRY_DIPLOMACY_H
#define WORLD_SIM_PANEL_COUNTRY_DIPLOMACY_H

#include "render/render_common.h"
#include "ui/ui_widgets.h"

int country_diplomacy_tab_height(int civ_id);
void draw_country_diplomacy_tab(HDC hdc, UiCursor *cursor, int civ_id);
int country_diplomacy_view_hit_test(int civ_id, RECT viewport, int scroll, int mouse_x, int mouse_y);

#endif
