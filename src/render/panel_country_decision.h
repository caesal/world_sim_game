#ifndef WORLD_SIM_PANEL_COUNTRY_DECISION_H
#define WORLD_SIM_PANEL_COUNTRY_DECISION_H

#include "platform/platform_types.h"

#include "ui/ui_widgets.h"

int country_decision_tab_height(int civ_id);
void draw_country_decision_tab(HDC hdc, UiCursor *cursor, int civ_id);

#endif
