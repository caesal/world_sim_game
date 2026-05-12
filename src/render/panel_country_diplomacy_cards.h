#ifndef WORLD_SIM_PANEL_COUNTRY_DIPLOMACY_CARDS_H
#define WORLD_SIM_PANEL_COUNTRY_DIPLOMACY_CARDS_H

#include "render/render_common.h"
#include "ui/ui_types.h"
#include "ui/ui_widgets.h"

void draw_diplomacy_relation_card(HDC hdc, UiCursor *cursor, int civ_id,
                                  int other_id, DiplomacyView view);

#endif
