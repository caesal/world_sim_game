#ifndef WORLD_SIM_PANEL_COUNTRY_H
#define WORLD_SIM_PANEL_COUNTRY_H

#include <windows.h>

#include "render/panel_country_actions.h"

#define COUNTRY_PANEL_HIT_NONE -1
#define COUNTRY_PANEL_HIT_TOGGLE_FALLEN -2
#define COUNTRY_PANEL_HIT_BACK_TO_LIST -3
#define COUNTRY_PANEL_HIT_CIVIL_UNREST -4
#define COUNTRY_PANEL_HIT_LOCATE -5
#define COUNTRY_PANEL_HIT_VASSAL_ACTION -6
#define COUNTRY_PANEL_HIT_COLOR -7
#define COUNTRY_PANEL_HIT_SORT_POPULATION -10
#define COUNTRY_PANEL_HIT_SORT_PROVINCES -11
#define COUNTRY_PANEL_HIT_SORT_ARMY -12
#define COUNTRY_PANEL_HIT_SORT_TREASURY -13
#define COUNTRY_PANEL_HIT_SORT_TECH -14
#define COUNTRY_PANEL_HIT_SORT_DISORDER -15
#define COUNTRY_PANEL_HIT_SUBTAB_BASE -30
#define COUNTRY_PANEL_HIT_DIPLOMACY_VIEW_BASE -60
#define COUNTRY_PANEL_HIT_DECISION_VIEW_BASE -80

int country_panel_hit_test(RECT client, int mouse_x, int mouse_y);
CountryVassalActionHit country_panel_vassal_action_target(RECT client, int mouse_x, int mouse_y);
int country_panel_scroll(RECT client, int delta);

#endif
