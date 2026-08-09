#ifndef WORLD_SIM_PANEL_COUNTRY_DECISION_H
#define WORLD_SIM_PANEL_COUNTRY_DECISION_H

#include <windows.h>

#include "sim/decision_snapshot.h"
#include "ui/ui_widgets.h"

typedef struct {
    RECT intent;
    RECT status;
    RECT composition;
    RECT factors[6];
    RECT factor_accents[6];
    RECT factor_labels[6];
    RECT factor_values[6];
    RECT total;
    RECT total_label;
    RECT total_value;
    int bottom;
} CountryDecisionStabilityLayout;

int country_decision_tab_height(int civ_id);
void draw_country_decision_tab(HDC hdc, UiCursor *cursor, int civ_id);
int country_decision_subtab_hit_test(RECT viewport, int scroll, int mouse_x, int mouse_y);
void draw_country_decision_subtabs(HDC hdc, UiCursor *cursor);
void draw_country_decision_stability_tab(HDC hdc, UiCursor *cursor, const DecisionSnapshot *snap);
void draw_country_decision_war_stability_summary(HDC hdc, UiCursor *cursor, const DecisionSnapshot *snap);
const char *country_decision_stability_mode_label(int mode);
void country_decision_stability_layout(
    int x, int y, int width, CountryDecisionStabilityLayout *layout);

#endif
