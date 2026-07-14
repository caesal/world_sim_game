#include "ui/ui_plague_panel.h"

#include "ui/ui_plague_panel_layout.h"
#include "ui/ui_plague_probability.h"

#include <string.h>

typedef struct {
    PlaguePanelTab main_tab;
    PlagueHistoryMetric history_metric;
    int scroll_offsets[PLAGUE_PANEL_TAB_COUNT];
    int content_heights[PLAGUE_PANEL_TAB_COUNT];
    int viewport_height;
    int impact_page;
    int impact_page_count;
    int history_episode_ids[UI_PLAGUE_HISTORY_SLOT_COUNT];
    int history_episode_count;
    int selected_history_episode_id;
    int hover_target;
    int hovered_history_episode_id;
    unsigned int cache_revision;
} UiPlaguePanelState;

static UiPlaguePanelState state = {
    PLAGUE_PANEL_TAB_LIVE,
    PLAGUE_HISTORY_METRIC_TYPE,
    {0, 0, 0},
    {0, 0, 0},
    0,
    0,
    0,
    {-1, -1, -1, -1, -1, -1, -1},
    0,
    -1,
    UI_PLAGUE_PANEL_HIT_NONE,
    -1,
    1u
};

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int valid_tab(PlaguePanelTab tab) {
    return tab >= PLAGUE_PANEL_TAB_LIVE && tab < PLAGUE_PANEL_TAB_COUNT;
}

static int valid_metric(PlagueHistoryMetric metric) {
    return metric >= PLAGUE_HISTORY_METRIC_TYPE &&
           metric < PLAGUE_HISTORY_METRIC_COUNT;
}

static void note_cache_change(void) {
    state.cache_revision++;
    if (state.cache_revision == 0u) state.cache_revision = 1u;
}

static int history_slot_for_episode(int episode_id) {
    int i;
    if (episode_id < 0) return -1;
    for (i = 0; i < state.history_episode_count; i++) {
        if (state.history_episode_ids[i] == episode_id) return i;
    }
    return -1;
}

PlaguePanelTab ui_plague_panel_main_tab(void) {
    return state.main_tab;
}

PlagueHistoryMetric ui_plague_panel_history_metric(void) {
    return state.history_metric;
}

int ui_plague_panel_impact_page(void) {
    return state.impact_page;
}

int ui_plague_panel_impact_page_count(void) {
    return state.impact_page_count;
}

int ui_plague_panel_scroll_offset(PlaguePanelTab tab) {
    return valid_tab(tab) ? state.scroll_offsets[tab] : 0;
}

int ui_plague_panel_content_height(PlaguePanelTab tab) {
    return valid_tab(tab) ? state.content_heights[tab] : 0;
}

int ui_plague_panel_selected_history_episode_id(void) {
    return state.selected_history_episode_id;
}

int ui_plague_panel_hovered_history_episode_id(void) {
    return state.hovered_history_episode_id;
}

int ui_plague_panel_hover_target(void) {
    return state.hover_target;
}

unsigned int ui_plague_panel_cache_revision(void) {
    return state.cache_revision;
}

int ui_plague_panel_set_main_tab(PlaguePanelTab tab) {
    if (!valid_tab(tab) || state.main_tab == tab) return 0;
    state.main_tab = tab;
    state.hover_target = UI_PLAGUE_PANEL_HIT_NONE;
    state.hovered_history_episode_id = -1;
    note_cache_change();
    return 1;
}

int ui_plague_panel_set_history_metric(PlagueHistoryMetric metric) {
    if (!valid_metric(metric) || state.history_metric == metric) return 0;
    state.history_metric = metric;
    note_cache_change();
    return 1;
}

int ui_plague_panel_set_impact_page(int page) {
    int maximum = max(0, state.impact_page_count - 1);
    int next = clamp_int(page, 0, maximum);
    if (state.impact_page == next) return 0;
    state.impact_page = next;
    note_cache_change();
    return 1;
}

int ui_plague_panel_set_impact_country_count(int country_count) {
    int page_count;
    int changed = 0;
    country_count = max(0, country_count);
    page_count = country_count > 0 ?
        (country_count + UI_PLAGUE_IMPACT_PAGE_SIZE - 1) /
        UI_PLAGUE_IMPACT_PAGE_SIZE : 0;
    if (state.impact_page_count != page_count) {
        state.impact_page_count = page_count;
        changed = 1;
    }
    if (state.impact_page >= page_count) {
        state.impact_page = max(0, page_count - 1);
        changed = 1;
    }
    if (changed) note_cache_change();
    return changed;
}

int ui_plague_panel_set_content_height(PlaguePanelTab tab, int height) {
    int maximum;
    int changed = 0;
    if (!valid_tab(tab)) return 0;
    height = max(0, height);
    if (state.content_heights[tab] != height) {
        state.content_heights[tab] = height;
        changed = 1;
    }
    maximum = max(0, height - state.viewport_height);
    if (state.scroll_offsets[tab] > maximum) {
        state.scroll_offsets[tab] = maximum;
        changed = 1;
    }
    if (changed) note_cache_change();
    return changed;
}

int ui_plague_panel_set_viewport_height(int height) {
    int changed = 0;
    int i;
    height = max(1, height);
    if (state.viewport_height != height) {
        state.viewport_height = height;
        changed = 1;
    }
    for (i = 0; i < PLAGUE_PANEL_TAB_COUNT; i++) {
        int maximum = max(0, state.content_heights[i] - height);
        if (state.scroll_offsets[i] > maximum) {
            state.scroll_offsets[i] = maximum;
            changed = 1;
        }
    }
    if (changed) note_cache_change();
    return changed;
}

int ui_plague_panel_set_history_episode_slots(const int *episode_ids,
                                               int count) {
    int next[UI_PLAGUE_HISTORY_SLOT_COUNT];
    int selected;
    int changed = 0;
    int i;

    count = clamp_int(count, 0, UI_PLAGUE_HISTORY_SLOT_COUNT);
    for (i = 0; i < UI_PLAGUE_HISTORY_SLOT_COUNT; i++) {
        next[i] = i < count && episode_ids ? episode_ids[i] : -1;
        if (state.history_episode_ids[i] != next[i]) changed = 1;
    }
    if (state.history_episode_count != count) changed = 1;
    memcpy(state.history_episode_ids, next, sizeof(next));
    state.history_episode_count = count;
    selected = state.selected_history_episode_id;
    if (history_slot_for_episode(selected) < 0) {
        selected = count > 0 ? state.history_episode_ids[count - 1] : -1;
    }
    if (state.selected_history_episode_id != selected) {
        state.selected_history_episode_id = selected;
        changed = 1;
    }
    if (state.hover_target >= UI_PLAGUE_PANEL_HIT_HISTORY_EPISODE_BASE &&
        state.hover_target < UI_PLAGUE_PANEL_HIT_HISTORY_EPISODE_BASE +
                             UI_PLAGUE_HISTORY_SLOT_COUNT) {
        int hover_slot = state.hover_target -
                         UI_PLAGUE_PANEL_HIT_HISTORY_EPISODE_BASE;
        state.hovered_history_episode_id =
            ui_plague_panel_history_slot_episode_id(hover_slot);
        if (state.hovered_history_episode_id < 0) {
            state.hover_target = UI_PLAGUE_PANEL_HIT_NONE;
        }
    }
    if (changed) note_cache_change();
    return changed;
}

int ui_plague_panel_select_history_episode(int episode_id) {
    if (history_slot_for_episode(episode_id) < 0 ||
        state.selected_history_episode_id == episode_id) return 0;
    state.selected_history_episode_id = episode_id;
    note_cache_change();
    return 1;
}

int ui_plague_panel_set_hover_target(int target) {
    int episode_id = -1;
    if (target >= UI_PLAGUE_PANEL_HIT_HISTORY_EPISODE_BASE &&
        target < UI_PLAGUE_PANEL_HIT_HISTORY_EPISODE_BASE +
                 UI_PLAGUE_HISTORY_SLOT_COUNT) {
        episode_id = ui_plague_panel_history_slot_episode_id(
            target - UI_PLAGUE_PANEL_HIT_HISTORY_EPISODE_BASE);
    }
    if (state.hover_target == target &&
        state.hovered_history_episode_id == episode_id) return 0;
    state.hover_target = target;
    state.hovered_history_episode_id = episode_id;
    return 1;
}

int ui_plague_panel_clear_hover(void) {
    return ui_plague_panel_set_hover_target(UI_PLAGUE_PANEL_HIT_NONE);
}

void ui_plague_panel_reset_presentation_state(void) {
    int i;
    memset(&state, 0, sizeof(state));
    state.main_tab = PLAGUE_PANEL_TAB_LIVE;
    state.history_metric = PLAGUE_HISTORY_METRIC_TYPE;
    state.selected_history_episode_id = -1;
    state.hover_target = UI_PLAGUE_PANEL_HIT_NONE;
    state.hovered_history_episode_id = -1;
    state.cache_revision = 1u;
    for (i = 0; i < UI_PLAGUE_HISTORY_SLOT_COUNT; i++) {
        state.history_episode_ids[i] = -1;
    }
    ui_plague_probability_reset_presentation_state();
}

int ui_plague_panel_history_slot_episode_id(int slot) {
    if (slot < 0 || slot >= state.history_episode_count) return -1;
    return state.history_episode_ids[slot];
}

static int point_in(RECT rect, int x, int y) {
    return x >= rect.left && x < rect.right &&
           y >= rect.top && y < rect.bottom;
}

int ui_plague_panel_hit_test(RECT client, int panel_width, int x, int y) {
    UiPlaguePanelLayout panel;
    int i;
    ui_plague_panel_layout_build(client, panel_width, &panel);
    i = ui_plague_probability_hit_test(&panel.probability, x, y);
    if (i != UI_PLAGUE_PANEL_HIT_NONE) return i;
    for (i = 0; i < PLAGUE_PANEL_TAB_COUNT; i++) {
        if (point_in(panel.main_tabs[i], x, y)) {
            return UI_PLAGUE_PANEL_HIT_MAIN_TAB_BASE + i;
        }
    }
    if (!point_in(panel.content_viewport, x, y)) {
        return UI_PLAGUE_PANEL_HIT_NONE;
    }
    if (state.main_tab == PLAGUE_PANEL_TAB_IMPACT) {
        UiPlagueImpactLayout impact;
        ui_plague_panel_impact_layout_build(&panel, &impact);
        if (point_in(impact.previous, x, y)) {
            return UI_PLAGUE_PANEL_HIT_IMPACT_PREVIOUS;
        }
        if (point_in(impact.next, x, y)) {
            return UI_PLAGUE_PANEL_HIT_IMPACT_NEXT;
        }
    } else if (state.main_tab == PLAGUE_PANEL_TAB_HISTORY) {
        UiPlagueHistoryLayout history;
        ui_plague_panel_history_layout_build(&panel, &history);
        for (i = 0; i < PLAGUE_HISTORY_METRIC_COUNT; i++) {
            if (point_in(history.metric_tabs[i], x, y)) {
                return UI_PLAGUE_PANEL_HIT_HISTORY_METRIC_BASE + i;
            }
        }
        for (i = 0; i < state.history_episode_count; i++) {
            if (point_in(history.episode_hits[i], x, y)) {
                return UI_PLAGUE_PANEL_HIT_HISTORY_EPISODE_BASE + i;
            }
        }
    }
    return UI_PLAGUE_PANEL_HIT_NONE;
}

int ui_plague_panel_handle_click(RECT client, int panel_width, int x, int y) {
    int hit = ui_plague_panel_hit_test(client, panel_width, x, y);
    if (hit >= UI_PLAGUE_PANEL_HIT_MAIN_TAB_BASE &&
        hit < UI_PLAGUE_PANEL_HIT_MAIN_TAB_BASE + PLAGUE_PANEL_TAB_COUNT) {
        ui_plague_panel_set_main_tab((PlaguePanelTab)(hit -
                                     UI_PLAGUE_PANEL_HIT_MAIN_TAB_BASE));
    } else if (hit == UI_PLAGUE_PANEL_HIT_IMPACT_PREVIOUS) {
        ui_plague_panel_set_impact_page(state.impact_page - 1);
    } else if (hit == UI_PLAGUE_PANEL_HIT_IMPACT_NEXT) {
        ui_plague_panel_set_impact_page(state.impact_page + 1);
    } else if (hit >= UI_PLAGUE_PANEL_HIT_HISTORY_METRIC_BASE &&
               hit < UI_PLAGUE_PANEL_HIT_HISTORY_METRIC_BASE +
                     PLAGUE_HISTORY_METRIC_COUNT) {
        ui_plague_panel_set_history_metric((PlagueHistoryMetric)(hit -
            UI_PLAGUE_PANEL_HIT_HISTORY_METRIC_BASE));
    } else if (hit >= UI_PLAGUE_PANEL_HIT_HISTORY_EPISODE_BASE &&
               hit < UI_PLAGUE_PANEL_HIT_HISTORY_EPISODE_BASE +
                     UI_PLAGUE_HISTORY_SLOT_COUNT) {
        ui_plague_panel_select_history_episode(
            ui_plague_panel_history_slot_episode_id(hit -
                UI_PLAGUE_PANEL_HIT_HISTORY_EPISODE_BASE));
    }
    return hit != UI_PLAGUE_PANEL_HIT_NONE;
}

int ui_plague_panel_scroll(RECT client, int panel_width, int delta) {
    UiPlaguePanelLayout layout;
    int before;
    int next;
    if (delta == 0) return 0;
    ui_plague_panel_layout_build(client, panel_width, &layout);
    before = state.scroll_offsets[state.main_tab];
    next = clamp_int(layout.scroll_offset + delta, 0, layout.max_scroll);
    if (before == next) return 0;
    state.scroll_offsets[state.main_tab] = next;
    note_cache_change();
    return 1;
}
