#ifndef WORLD_SIM_PANEL_WAR_COMPARE_BAR_H
#define WORLD_SIM_PANEL_WAR_COMPARE_BAR_H

#include "render/render_common.h"
#include "ui/ui_widgets.h"

typedef struct {
    const char *left_name;
    const char *right_name;
    const char *left_role;
    const char *right_role;
    int left_regular;
    int left_vassal;
    int left_mercenary;
    int left_alliance;
    int right_regular;
    int right_vassal;
    int right_mercenary;
    int right_alliance;
    COLORREF left_regular_color;
    COLORREF left_vassal_color;
    COLORREF left_alliance_color;
    COLORREF right_regular_color;
    COLORREF right_vassal_color;
    COLORREF right_alliance_color;
} WarCompareBarModel;

int panel_war_compare_bar_height(const WarCompareBarModel *model);
void panel_war_compare_bar_draw(HDC hdc, UiCursor *cursor, const WarCompareBarModel *model);
int panel_war_compare_bar_probe_render(const char *path, int scenario);
int panel_war_compare_bar_probe_totals(int scenario, int *left_regular,
                                       int *left_alliance, int *right_regular,
                                       int *right_alliance, int *left_total,
                                       int *right_total);

#endif
