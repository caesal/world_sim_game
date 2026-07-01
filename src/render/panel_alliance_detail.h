#ifndef WORLD_SIM_PANEL_ALLIANCE_DETAIL_H
#define WORLD_SIM_PANEL_ALLIANCE_DETAIL_H

#include "render/panel_alliance_model.h"
#include "ui/ui_widgets.h"

typedef struct {
    RECT buttons[ALLIANCE_MEMBER_SORT_COUNT];
} AllianceMemberSortLayout;

typedef enum {
    ALLIANCE_OVERVIEW_ACTION_NONE = 0,
    ALLIANCE_OVERVIEW_ACTION_INVITE,
    ALLIANCE_OVERVIEW_ACTION_REMOVE
} AllianceOverviewAction;

const char *alliance_detail_tab_label(int tab);
const char *alliance_detail_vote_type_label(int type);
const char *alliance_detail_reason_label(int reason);
int alliance_detail_content_height(const RenderSnapshot *snapshot, const AlliancePanelRow *row);
void alliance_detail_draw_member_sort(HDC hdc, const AllianceMemberSortLayout *layout);
void alliance_detail_draw_content(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                                  const AlliancePanelRow *row);
AllianceOverviewAction alliance_detail_overview_action_hit(int alliance_id, int mouse_x, int mouse_y);

#endif
