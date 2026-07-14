#ifndef WORLD_SIM_UI_PLAGUE_PANEL_LAYOUT_H
#define WORLD_SIM_UI_PLAGUE_PANEL_LAYOUT_H

#include "ui/ui_plague_fog.h"
#include "ui/ui_plague_panel.h"
#include "ui/ui_plague_probability.h"

typedef struct {
    PlaguePanelLayout fog;
    UiPlagueProbabilityLayout probability;
    RECT main_tabs[PLAGUE_PANEL_TAB_COUNT];
    RECT content_viewport;
    int content_origin_y;
    int scroll_offset;
    int max_scroll;
} UiPlaguePanelLayout;

typedef struct {
    RECT country_header;
    RECT country_note;
    RECT pager;
    RECT previous;
    RECT page_label;
    RECT next;
    RECT country_rows[UI_PLAGUE_IMPACT_PAGE_SIZE];
    RECT city_header;
    RECT city_rows[UI_PLAGUE_CITY_TOP_COUNT];
    int content_height;
} UiPlagueImpactLayout;

typedef struct {
    RECT metric_tabs[PLAGUE_HISTORY_METRIC_COUNT];
    RECT chart;
    RECT plot;
    RECT episode_hits[UI_PLAGUE_HISTORY_SLOT_COUNT];
    int episode_x[UI_PLAGUE_HISTORY_SLOT_COUNT];
    RECT detail_strip;
    int content_height;
} UiPlagueHistoryLayout;

void ui_plague_panel_layout_build(RECT client, int panel_width,
                                  UiPlaguePanelLayout *layout);
void ui_plague_panel_impact_layout_build(const UiPlaguePanelLayout *panel,
                                         UiPlagueImpactLayout *layout);
void ui_plague_panel_history_layout_build(const UiPlaguePanelLayout *panel,
                                          UiPlagueHistoryLayout *layout);

#endif
