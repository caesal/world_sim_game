#ifndef WORLD_SIM_PANEL_COUNTRY_ACTIONS_H
#define WORLD_SIM_PANEL_COUNTRY_ACTIONS_H

#define WIN32_LEAN_AND_MEAN
#include "platform/platform_types.h"

#include "ui/ui_widgets.h"

void country_overview_actions_reset_hit(void);
int country_overview_actions_height(int civ_id);
void country_overview_vassal_actions_reset_hit(void);
int country_overview_vassal_actions_height(int civ_id);
int country_overview_civil_unrest_hit(RECT viewport, int mouse_x, int mouse_y);
int country_overview_vassal_action_hit(RECT viewport, int mouse_x, int mouse_y);
void draw_country_overview_actions(HDC hdc, UiCursor *cursor, int civ_id);
void draw_country_overview_vassal_actions(HDC hdc, UiCursor *cursor, int civ_id);

#endif
