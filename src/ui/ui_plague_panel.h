#ifndef WORLD_SIM_UI_PLAGUE_PANEL_H
#define WORLD_SIM_UI_PLAGUE_PANEL_H

#include <windows.h>

#define UI_PLAGUE_IMPACT_PAGE_SIZE 6
#define UI_PLAGUE_CITY_TOP_COUNT 5
#define UI_PLAGUE_HISTORY_SLOT_COUNT 7

typedef enum {
    PLAGUE_PANEL_TAB_LIVE,
    PLAGUE_PANEL_TAB_IMPACT,
    PLAGUE_PANEL_TAB_HISTORY,
    PLAGUE_PANEL_TAB_COUNT
} PlaguePanelTab;

typedef enum {
    PLAGUE_HISTORY_METRIC_TYPE,
    PLAGUE_HISTORY_METRIC_SEVERITY,
    PLAGUE_HISTORY_METRIC_DURATION,
    PLAGUE_HISTORY_METRIC_DEATHS,
    PLAGUE_HISTORY_METRIC_CITIES,
    PLAGUE_HISTORY_METRIC_COUNTRIES,
    PLAGUE_HISTORY_METRIC_SPORES,
    PLAGUE_HISTORY_METRIC_COUNT
} PlagueHistoryMetric;

typedef enum {
    UI_PLAGUE_PANEL_HIT_NONE = 0,
    UI_PLAGUE_PANEL_HIT_MAIN_TAB_BASE = 100,
    UI_PLAGUE_PANEL_HIT_IMPACT_PREVIOUS = 200,
    UI_PLAGUE_PANEL_HIT_IMPACT_NEXT = 201,
    UI_PLAGUE_PANEL_HIT_HISTORY_METRIC_BASE = 300,
    UI_PLAGUE_PANEL_HIT_HISTORY_EPISODE_BASE = 400,
    UI_PLAGUE_PANEL_HIT_PROBABILITY_BASE = 500,
    UI_PLAGUE_PANEL_HIT_PROBABILITY_APPLY = 510,
    UI_PLAGUE_PANEL_HIT_PROBABILITY_RESET = 511
} UiPlaguePanelHit;

PlaguePanelTab ui_plague_panel_main_tab(void);
PlagueHistoryMetric ui_plague_panel_history_metric(void);
int ui_plague_panel_impact_page(void);
int ui_plague_panel_impact_page_count(void);
int ui_plague_panel_scroll_offset(PlaguePanelTab tab);
int ui_plague_panel_content_height(PlaguePanelTab tab);
int ui_plague_panel_selected_history_episode_id(void);
int ui_plague_panel_hovered_history_episode_id(void);
int ui_plague_panel_hover_target(void);
unsigned int ui_plague_panel_cache_revision(void);

int ui_plague_panel_set_main_tab(PlaguePanelTab tab);
int ui_plague_panel_set_history_metric(PlagueHistoryMetric metric);
int ui_plague_panel_set_impact_page(int page);
int ui_plague_panel_set_impact_country_count(int country_count);
int ui_plague_panel_set_content_height(PlaguePanelTab tab, int height);
int ui_plague_panel_set_viewport_height(int height);
int ui_plague_panel_set_history_episode_slots(const int *episode_ids,
                                               int count);
int ui_plague_panel_select_history_episode(int episode_id);
int ui_plague_panel_set_hover_target(int target);
int ui_plague_panel_clear_hover(void);
void ui_plague_panel_reset_presentation_state(void);

int ui_plague_panel_history_slot_episode_id(int slot);
int ui_plague_panel_hit_test(RECT client, int panel_width, int x, int y);
int ui_plague_panel_handle_click(RECT client, int panel_width, int x, int y);
int ui_plague_panel_scroll(RECT client, int panel_width, int delta);

#endif
