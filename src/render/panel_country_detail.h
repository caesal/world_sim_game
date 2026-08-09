#ifndef WORLD_SIM_PANEL_COUNTRY_DETAIL_H
#define WORLD_SIM_PANEL_COUNTRY_DETAIL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "render/panel_country_actions.h"
#include "sim/decision_snapshot.h"
#include "ui/ui_widgets.h"

int country_detail_content_height(int civ_id);
void country_detail_reset_hit(void);
int country_detail_civil_unrest_hit(RECT viewport, int mouse_x, int mouse_y);
CountryVassalActionHit country_detail_vassal_action_hit(RECT viewport, int mouse_x, int mouse_y);
const char *country_overview_next_action_text(
    const DecisionSnapshot *decision, int language);
void draw_country_detail_content(HDC hdc, UiCursor *cursor, RECT viewport,
                                 int scroll, int civ_id,
                                 HFONT title_font, HFONT body_font);

#endif
