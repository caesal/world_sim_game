#ifndef WORLD_SIM_PANEL_COUNTRY_CARDS_H
#define WORLD_SIM_PANEL_COUNTRY_CARDS_H

#include "platform/platform_types.h"

void draw_country_summary_card(HDC hdc, RECT rect, int civ_id, int selected);
void draw_country_selected_summary(HDC hdc, RECT rect, int civ_id);

#endif
