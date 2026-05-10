#ifndef WORLD_SIM_PANEL_COUNTRY_RESOURCES_H
#define WORLD_SIM_PANEL_COUNTRY_RESOURCES_H

#include "render/render_common.h"
#include "ui/ui_widgets.h"

int country_resources_tab_height(int civ_id);
void draw_country_resources_tab(HDC hdc, UiCursor *cursor, int civ_id);

#endif
