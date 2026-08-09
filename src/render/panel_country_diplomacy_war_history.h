#ifndef WORLD_SIM_PANEL_COUNTRY_DIPLOMACY_WAR_HISTORY_H
#define WORLD_SIM_PANEL_COUNTRY_DIPLOMACY_WAR_HISTORY_H

#include "core/render_snapshot.h"
#include "render/render_common.h"
#include "ui/ui_widgets.h"

typedef struct {
    RECT card;
    RECT title;
    RECT title_text;
    RECT matchup;
    RECT local_swatch;
    RECT local_name;
    RECT local_casualties;
    RECT result;
    RECT opponent_swatch;
    RECT opponent_name;
    RECT opponent_casualties;
    RECT footer;
    RECT settlement;
    RECT date;
    int line_height;
    int wide;
    int footer_rows;
} WarHistoryCardLayout;

int country_diplomacy_war_history_height(const SnapshotCiv *civ);
int country_diplomacy_war_history_card_height(int panel_width,
                                               int line_height);
int country_diplomacy_war_history_record_text_fits(
    HDC hdc, int panel_width, int card_width,
    const SnapshotWarHistoryRecord *record);
void war_history_card_layout(RECT card, WarHistoryCardLayout *layout);
void war_history_card_layout_for_metrics(
    RECT card, int panel_width, int line_height,
    WarHistoryCardLayout *layout);
void draw_country_diplomacy_war_history(HDC hdc, UiCursor *cursor,
                                        const SnapshotCiv *civ);

#endif
