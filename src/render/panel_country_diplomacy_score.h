#ifndef WORLD_SIM_PANEL_COUNTRY_DIPLOMACY_SCORE_H
#define WORLD_SIM_PANEL_COUNTRY_DIPLOMACY_SCORE_H

#include "render/render_common.h"
#include "ui/ui_widgets.h"

void draw_diplomacy_relation_score_block(HDC hdc, UiCursor *cursor, int civ_id, int other_id);
int diplomacy_relation_score_block_height(int civ_id, int other_id);

#endif
