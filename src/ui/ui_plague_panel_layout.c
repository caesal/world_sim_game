#include "ui/ui_plague_panel_layout.h"

#include "core/constants.h"

#include <string.h>

enum {
    PLAGUE_MAIN_TAB_HEIGHT = 28,
    PLAGUE_MAIN_TAB_GAP = 4,
    PLAGUE_CONTENT_TOP_GAP = 10,
    PLAGUE_IMPACT_HEADER_HEIGHT = 24,
    PLAGUE_IMPACT_PAGER_HEIGHT = 30,
    PLAGUE_IMPACT_PAGER_BUTTON_SIZE = 30,
    PLAGUE_IMPACT_ROW_HEIGHT = 70,
    PLAGUE_IMPACT_ROW_GAP = 8,
    PLAGUE_CITY_ROW_HEIGHT = 54,
    PLAGUE_CITY_ROW_GAP = 7,
    PLAGUE_HISTORY_METRIC_HEIGHT = 28,
    PLAGUE_HISTORY_METRIC_GAP = 4,
    PLAGUE_HISTORY_CHART_HEIGHT = 250,
    PLAGUE_HISTORY_DETAIL_HEIGHT = 118
};

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static RECT tab_rect(int left, int right, int top, int count, int index) {
    int available = right - left - PLAGUE_MAIN_TAB_GAP * (count - 1);
    int x0 = left + available * index / count + PLAGUE_MAIN_TAB_GAP * index;
    int x1 = left + available * (index + 1) / count +
             PLAGUE_MAIN_TAB_GAP * index;
    if (index == count - 1) x1 = right;
    return (RECT){x0, top, x1, top + PLAGUE_MAIN_TAB_HEIGHT};
}

void ui_plague_panel_layout_build(RECT client, int panel_width,
                                  UiPlaguePanelLayout *layout) {
    PlaguePanelTab tab;
    int left;
    int right;
    int content_height;
    int viewport_height;
    int scroll;
    int i;

    if (!layout) return;
    memset(layout, 0, sizeof(*layout));
    panel_width = clamp_int(panel_width, 1, max(1, client.right - client.left));
    ui_plague_fog_layout_build(client, panel_width, &layout->fog);
    ui_plague_probability_layout_build(client, panel_width,
                                       layout->fog.content_top,
                                       &layout->probability);
    left = client.right - panel_width + FORM_X_PAD;
    right = max(left + 1, client.right - FORM_X_PAD);
    for (i = 0; i < PLAGUE_PANEL_TAB_COUNT; i++) {
        layout->main_tabs[i] = tab_rect(left, right,
                                        layout->probability.bottom,
                                        PLAGUE_PANEL_TAB_COUNT, i);
    }
    layout->content_viewport = (RECT){left,
        layout->main_tabs[0].bottom + PLAGUE_CONTENT_TOP_GAP,
        right, client.bottom - BOTTOM_BAR_H};
    if (layout->content_viewport.bottom < layout->content_viewport.top) {
        layout->content_viewport.bottom = layout->content_viewport.top;
    }
    tab = ui_plague_panel_main_tab();
    content_height = max(0, ui_plague_panel_content_height(tab));
    viewport_height = max(1, layout->content_viewport.bottom -
                          layout->content_viewport.top);
    ui_plague_panel_set_viewport_height(viewport_height);
    layout->max_scroll = max(0, content_height - viewport_height);
    scroll = ui_plague_panel_scroll_offset(tab);
    layout->scroll_offset = clamp_int(scroll, 0, layout->max_scroll);
    layout->content_origin_y = layout->content_viewport.top -
                               layout->scroll_offset;
}

void ui_plague_panel_impact_layout_build(const UiPlaguePanelLayout *panel,
                                         UiPlagueImpactLayout *layout) {
    int x;
    int right;
    int y;
    int i;

    if (!layout) return;
    memset(layout, 0, sizeof(*layout));
    if (!panel) return;
    x = panel->content_viewport.left;
    right = panel->content_viewport.right;
    y = panel->content_origin_y;
    layout->country_header = (RECT){x, y, right,
        y + PLAGUE_IMPACT_HEADER_HEIGHT};
    y = layout->country_header.bottom + 4;
    layout->country_note = (RECT){x, y, right, y + 34};
    y = layout->country_note.bottom + 6;
    layout->pager = (RECT){x, y, right, y + PLAGUE_IMPACT_PAGER_HEIGHT};
    layout->previous = (RECT){x, y,
        x + PLAGUE_IMPACT_PAGER_BUTTON_SIZE, layout->pager.bottom};
    layout->next = (RECT){right - PLAGUE_IMPACT_PAGER_BUTTON_SIZE, y, right,
                          layout->pager.bottom};
    layout->page_label = (RECT){layout->previous.right + 4, y,
        layout->next.left - 4, layout->pager.bottom};
    y = layout->pager.bottom + 10;
    for (i = 0; i < UI_PLAGUE_IMPACT_PAGE_SIZE; i++) {
        layout->country_rows[i] = (RECT){x, y, right,
                                         y + PLAGUE_IMPACT_ROW_HEIGHT};
        y += PLAGUE_IMPACT_ROW_HEIGHT + PLAGUE_IMPACT_ROW_GAP;
    }
    y += 2;
    layout->city_header = (RECT){x, y, right,
        y + PLAGUE_IMPACT_HEADER_HEIGHT};
    y = layout->city_header.bottom + 6;
    for (i = 0; i < UI_PLAGUE_CITY_TOP_COUNT; i++) {
        layout->city_rows[i] = (RECT){x, y, right,
                                      y + PLAGUE_CITY_ROW_HEIGHT};
        y += PLAGUE_CITY_ROW_HEIGHT + PLAGUE_CITY_ROW_GAP;
    }
    layout->content_height = y - panel->content_origin_y + 8;
}

static void build_metric_row(RECT *tabs, int first, int count, int left,
                             int right, int top) {
    int available = right - left - PLAGUE_HISTORY_METRIC_GAP * (count - 1);
    int i;
    for (i = 0; i < count; i++) {
        int x0 = left + available * i / count +
                 PLAGUE_HISTORY_METRIC_GAP * i;
        int x1 = left + available * (i + 1) / count +
                 PLAGUE_HISTORY_METRIC_GAP * i;
        if (i == count - 1) x1 = right;
        tabs[first + i] = (RECT){x0, top, x1,
            top + PLAGUE_HISTORY_METRIC_HEIGHT};
    }
}

void ui_plague_panel_history_layout_build(const UiPlaguePanelLayout *panel,
                                          UiPlagueHistoryLayout *layout) {
    int left;
    int right;
    int y;
    int plot_width;
    int i;

    if (!layout) return;
    memset(layout, 0, sizeof(*layout));
    if (!panel) return;
    left = panel->content_viewport.left;
    right = panel->content_viewport.right;
    y = panel->content_origin_y;
    build_metric_row(layout->metric_tabs, 0, 4, left, right, y);
    y += PLAGUE_HISTORY_METRIC_HEIGHT + PLAGUE_HISTORY_METRIC_GAP;
    build_metric_row(layout->metric_tabs, 4, 3, left, right, y);
    y += PLAGUE_HISTORY_METRIC_HEIGHT + 12;
    layout->chart = (RECT){left, y, right, y + PLAGUE_HISTORY_CHART_HEIGHT};
    layout->plot = (RECT){left + 58, y + 18, right - 8,
                          layout->chart.bottom - 34};
    if (layout->plot.right < layout->plot.left) {
        layout->plot.right = layout->plot.left;
    }
    plot_width = max(1, layout->plot.right - layout->plot.left);
    for (i = 0; i < UI_PLAGUE_HISTORY_SLOT_COUNT; i++) {
        int hit_left = layout->plot.left + plot_width * i /
                       UI_PLAGUE_HISTORY_SLOT_COUNT;
        int hit_right = layout->plot.left + plot_width * (i + 1) /
                        UI_PLAGUE_HISTORY_SLOT_COUNT;
        layout->episode_x[i] = layout->plot.left +
            plot_width * (2 * i + 1) / (2 * UI_PLAGUE_HISTORY_SLOT_COUNT);
        layout->episode_hits[i] = (RECT){hit_left, layout->plot.top,
                                         max(hit_left + 1, hit_right),
                                         layout->chart.bottom};
    }
    y = layout->chart.bottom + 10;
    layout->detail_strip = (RECT){left, y, right,
                                   y + PLAGUE_HISTORY_DETAIL_HEIGHT};
    layout->content_height = layout->detail_strip.bottom -
                             panel->content_origin_y + 8;
}
