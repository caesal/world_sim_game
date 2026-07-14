#include "game/game_presentation_plague_probe.h"

#include "core/game_types.h"
#include "core/render_snapshot.h"
#include "render/panel_view_model_cache_keys.h"
#include "ui/ui_layout.h"
#include "ui/ui_panel_hover.h"
#include "ui/ui_plague_fog.h"
#include "ui/ui_plague_panel.h"
#include "ui/ui_plague_panel_layout.h"
#include "ui/ui_pressed_state.h"
#include "ui/ui_types.h"

#include <stdlib.h>

static int center_x(RECT rect) { return (rect.left + rect.right) / 2; }
static int center_y(RECT rect) { return (rect.top + rect.bottom) / 2; }

static int fixed_hover_target_case(RECT client, int panel_width) {
    PlaguePanelLayout fog;
    int fog_key;
    int outside_key;
    int first_tab;
    int second_tab;
    ui_plague_fog_layout_build(client, panel_width, &fog);
    fog_key = ui_panel_hover_target_key(
        client, center_x(fog.slider.hit), center_y(fog.slider.hit));
    outside_key = ui_panel_hover_target_key(
        client, center_x(fog.slider.hit), fog.slider.hit.top - 8);
    first_tab = ui_panel_hover_target_key(
        client, center_x(get_panel_tab_rect(client, 0)),
        center_y(get_panel_tab_rect(client, 0)));
    second_tab = ui_panel_hover_target_key(
        client, center_x(get_panel_tab_rect(client, 1)),
        center_y(get_panel_tab_rect(client, 1)));
    return fog_key != outside_key && first_tab != second_tab &&
           fog_key != first_tab && fog_key != second_tab;
}

static int pressed_state_case(void) {
    int ok = 1;
    ui_pressed_control_clear(NULL);
    ui_pressed_control_set(NULL, UI_PRESSED_PLAGUE_MAIN_TAB,
                           PLAGUE_PANEL_TAB_IMPACT);
    ok &= ui_pressed_control_is_active(
        UI_PRESSED_PLAGUE_MAIN_TAB, PLAGUE_PANEL_TAB_IMPACT);
    ui_pressed_control_set(NULL, UI_PRESSED_PLAGUE_IMPACT_PAGER, 1);
    ok &= ui_pressed_control_is_active(UI_PRESSED_PLAGUE_IMPACT_PAGER, 1);
    ok &= !ui_pressed_control_is_active(
        UI_PRESSED_PLAGUE_MAIN_TAB, PLAGUE_PANEL_TAB_IMPACT);
    ui_pressed_control_set(NULL, UI_PRESSED_PLAGUE_HISTORY_METRIC,
                           PLAGUE_HISTORY_METRIC_DURATION);
    ok &= ui_pressed_control_is_active(
        UI_PRESSED_PLAGUE_HISTORY_METRIC,
        PLAGUE_HISTORY_METRIC_DURATION);
    ui_pressed_control_clear(NULL);
    return ok && !ui_pressed_control_is_active(
        UI_PRESSED_PLAGUE_HISTORY_METRIC,
        PLAGUE_HISTORY_METRIC_DURATION);
}

static int independent_scroll_case(RECT client, int panel_width) {
    int live;
    int impact;
    int history;
    int preserved;
    int clamped;
    ui_plague_panel_reset_presentation_state();
    ui_plague_panel_set_content_height(PLAGUE_PANEL_TAB_LIVE, 2400);
    ui_plague_panel_set_content_height(PLAGUE_PANEL_TAB_IMPACT, 2400);
    ui_plague_panel_set_content_height(PLAGUE_PANEL_TAB_HISTORY, 2400);
    ui_plague_panel_scroll(client, panel_width, 72);
    live = ui_plague_panel_scroll_offset(PLAGUE_PANEL_TAB_LIVE);
    ui_plague_panel_set_main_tab(PLAGUE_PANEL_TAB_IMPACT);
    ui_plague_panel_scroll(client, panel_width, 144);
    impact = ui_plague_panel_scroll_offset(PLAGUE_PANEL_TAB_IMPACT);
    ui_plague_panel_set_impact_country_count(13);
    ui_plague_panel_set_impact_page(2);
    ui_plague_panel_set_main_tab(PLAGUE_PANEL_TAB_HISTORY);
    ui_plague_panel_scroll(client, panel_width, 216);
    history = ui_plague_panel_scroll_offset(PLAGUE_PANEL_TAB_HISTORY);
    ui_plague_panel_set_history_metric(PLAGUE_HISTORY_METRIC_DURATION);
    ui_plague_panel_set_main_tab(PLAGUE_PANEL_TAB_LIVE);
    preserved = ui_plague_panel_scroll_offset(PLAGUE_PANEL_TAB_LIVE) == live;
    ui_plague_panel_set_content_height(PLAGUE_PANEL_TAB_LIVE, 100);
    clamped = ui_plague_panel_scroll_offset(PLAGUE_PANEL_TAB_LIVE) == 0;
    ui_plague_panel_set_content_height(PLAGUE_PANEL_TAB_LIVE, 2400);
    clamped &= ui_plague_panel_scroll_offset(PLAGUE_PANEL_TAB_LIVE) == 0;
    return live == 72 && impact == 144 && history == 216 &&
           preserved && clamped &&
           ui_plague_panel_impact_page() == 2 &&
           ui_plague_panel_history_metric() ==
               PLAGUE_HISTORY_METRIC_DURATION;
}

static int pagination_case(RECT client, int panel_width) {
    UiPlaguePanelLayout panel;
    UiPlagueImpactLayout impact;
    int page_two;
    int row_geometry;
    ui_plague_panel_reset_presentation_state();
    ui_plague_panel_set_main_tab(PLAGUE_PANEL_TAB_IMPACT);
    ui_plague_panel_set_impact_country_count(13);
    ui_plague_panel_set_content_height(PLAGUE_PANEL_TAB_IMPACT, 900);
    ui_plague_panel_layout_build(client, panel_width, &panel);
    ui_plague_panel_impact_layout_build(&panel, &impact);
    row_geometry = UI_PLAGUE_IMPACT_PAGE_SIZE == 6 &&
        impact.country_rows[0].top < impact.country_rows[5].top &&
        impact.country_rows[5].bottom <= impact.city_header.top;
    if (!ui_plague_panel_handle_click(client, panel_width,
            center_x(impact.next), center_y(impact.next))) return 0;
    if (ui_plague_panel_impact_page() != 1) return 0;
    ui_plague_panel_handle_click(client, panel_width,
        center_x(impact.next), center_y(impact.next));
    page_two = ui_plague_panel_impact_page() == 2;
    ui_plague_panel_handle_click(client, panel_width,
        center_x(impact.next), center_y(impact.next));
    if (ui_plague_panel_impact_page() != 2) return 0;
    ui_plague_panel_handle_click(client, panel_width,
        center_x(impact.previous), center_y(impact.previous));
    return row_geometry && page_two &&
           ui_plague_panel_impact_page_count() == 3 &&
           ui_plague_panel_impact_page() == 1;
}

static int hitbox_case(RECT client, int panel_width) {
    UiPlaguePanelLayout panel;
    UiPlagueHistoryLayout history;
    int ids[7] = {101, 102, 103, 104, 105, 106, 107};
    int metric_hit;
    int episode_hit;
    int hover;
    ui_plague_panel_reset_presentation_state();
    ui_plague_panel_layout_build(client, panel_width, &panel);
    if (!ui_plague_panel_handle_click(client, panel_width,
            center_x(panel.main_tabs[PLAGUE_PANEL_TAB_HISTORY]),
            center_y(panel.main_tabs[PLAGUE_PANEL_TAB_HISTORY])) ||
        ui_plague_panel_main_tab() != PLAGUE_PANEL_TAB_HISTORY) return 0;
    ui_plague_panel_set_history_episode_slots(ids, 7);
    ui_plague_panel_set_content_height(PLAGUE_PANEL_TAB_HISTORY, 520);
    ui_plague_panel_layout_build(client, panel_width, &panel);
    ui_plague_panel_history_layout_build(&panel, &history);
    metric_hit = ui_plague_panel_hit_test(client, panel_width,
        center_x(history.metric_tabs[PLAGUE_HISTORY_METRIC_DEATHS]),
        center_y(history.metric_tabs[PLAGUE_HISTORY_METRIC_DEATHS]));
    if (metric_hit != UI_PLAGUE_PANEL_HIT_HISTORY_METRIC_BASE +
                      PLAGUE_HISTORY_METRIC_DEATHS) return 0;
    ui_plague_panel_handle_click(client, panel_width,
        center_x(history.metric_tabs[PLAGUE_HISTORY_METRIC_DEATHS]),
        center_y(history.metric_tabs[PLAGUE_HISTORY_METRIC_DEATHS]));
    if (ui_plague_panel_history_metric() !=
        PLAGUE_HISTORY_METRIC_DEATHS) return 0;
    episode_hit = ui_plague_panel_hit_test(client, panel_width,
        center_x(history.episode_hits[2]), center_y(history.episode_hits[2]));
    if (episode_hit != UI_PLAGUE_PANEL_HIT_HISTORY_EPISODE_BASE + 2) return 0;
    ui_plague_panel_handle_click(client, panel_width,
        center_x(history.episode_hits[2]), center_y(history.episode_hits[2]));
    if (ui_plague_panel_selected_history_episode_id() != 103) return 0;
    hover = ui_plague_panel_hit_test(client, panel_width,
        center_x(history.episode_hits[3]), center_y(history.episode_hits[3]));
    ui_plague_panel_set_hover_target(hover);
    if (ui_plague_panel_hovered_history_episode_id() != 104 ||
        ui_plague_panel_hover_target() != hover) return 0;
    ui_plague_panel_clear_hover();
    return ui_plague_panel_hovered_history_episode_id() == -1 &&
           ui_plague_panel_hover_target() == UI_PLAGUE_PANEL_HIT_NONE;
}

static int cache_ui_key_case(RECT client) {
    int ids[7] = {101, 102, 103, 104, 105, 106, 107};
    unsigned int baseline;
    unsigned int changed;
    unsigned int before;
    ui_plague_panel_reset_presentation_state();
    baseline = panel_view_model_cache_ui_key(client, PANEL_CACHE_PLAGUE);
    if (ui_plague_panel_set_main_tab(PLAGUE_PANEL_TAB_LIVE) != 0 ||
        panel_view_model_cache_ui_key(client, PANEL_CACHE_PLAGUE) != baseline) {
        return 0;
    }
    ui_plague_panel_set_content_height(PLAGUE_PANEL_TAB_LIVE, 2400);
    changed = panel_view_model_cache_ui_key(client, PANEL_CACHE_PLAGUE);
    if (changed == baseline) {
        return 0;
    }
    baseline = changed;
    ui_plague_panel_set_hover_target(UI_PLAGUE_PANEL_HIT_MAIN_TAB_BASE + 1);
    if (panel_view_model_cache_ui_key(client, PANEL_CACHE_PLAGUE) != baseline) {
        return 0;
    }
    ui_plague_panel_clear_hover();
    ui_plague_panel_set_main_tab(PLAGUE_PANEL_TAB_IMPACT);
    changed = panel_view_model_cache_ui_key(client, PANEL_CACHE_PLAGUE);
    if (changed == baseline) return 0;
    ui_plague_panel_set_impact_country_count(13);
    before = panel_view_model_cache_ui_key(client, PANEL_CACHE_PLAGUE);
    ui_plague_panel_set_impact_page(1);
    if (panel_view_model_cache_ui_key(client, PANEL_CACHE_PLAGUE) == before) {
        return 0;
    }
    ui_plague_panel_set_main_tab(PLAGUE_PANEL_TAB_HISTORY);
    before = panel_view_model_cache_ui_key(client, PANEL_CACHE_PLAGUE);
    ui_plague_panel_set_history_metric(PLAGUE_HISTORY_METRIC_COUNTRIES);
    if (panel_view_model_cache_ui_key(client, PANEL_CACHE_PLAGUE) == before) {
        return 0;
    }
    ui_plague_panel_set_history_episode_slots(ids, 7);
    before = panel_view_model_cache_ui_key(client, PANEL_CACHE_PLAGUE);
    ui_plague_panel_select_history_episode(104);
    if (panel_view_model_cache_ui_key(client, PANEL_CACHE_PLAGUE) == before) {
        return 0;
    }
    ui_plague_panel_set_content_height(PLAGUE_PANEL_TAB_HISTORY, 2400);
    before = panel_view_model_cache_ui_key(client, PANEL_CACHE_PLAGUE);
    ui_plague_panel_scroll(client, side_panel_w, 72);
    return panel_view_model_cache_ui_key(client, PANEL_CACHE_PLAGUE) != before;
}

static int cache_data_key_case(void) {
    RenderSnapshot *snapshot = calloc(1, sizeof(RenderSnapshot));
    unsigned int baseline;
    int ok;
    if (!snapshot) return 0;
    snapshot->world_generated = 1;
    snapshot->map_w = 64;
    snapshot->map_h = 36;
    snapshot->plague_revision = 10;
    snapshot->civs_revision = 20;
    snapshot->cities_revision = 30;
    baseline = panel_view_model_cache_data_key(snapshot, PANEL_CACHE_PLAGUE);
    snapshot->events_revision++;
    ok = panel_view_model_cache_data_key(snapshot, PANEL_CACHE_PLAGUE) == baseline;
    snapshot->plague_revision++;
    ok &= panel_view_model_cache_data_key(snapshot, PANEL_CACHE_PLAGUE) != baseline;
    snapshot->plague_revision--;
    snapshot->civs_revision++;
    ok &= panel_view_model_cache_data_key(snapshot, PANEL_CACHE_PLAGUE) != baseline;
    snapshot->civs_revision--;
    snapshot->cities_revision++;
    ok &= panel_view_model_cache_data_key(snapshot, PANEL_CACHE_PLAGUE) != baseline;
    free(snapshot);
    return ok;
}

int game_presentation_plague_interaction_probe(FILE *summary) {
    RECT client = {0, 0, 720, 1040};
    int old_language = ui_language;
    int old_width = side_panel_w;
    int old_collapsed = side_panel_collapsed;
    int old_panel = panel_tab;
    int old_fog = plague_fog_alpha;
    int scroll;
    int pagination;
    int hitboxes;
    int fixed_hover;
    int pressed;
    int ui_keys;
    int data_keys;
    int ok;
    ui_language = UI_LANG_EN;
    side_panel_w = 460;
    side_panel_collapsed = 0;
    panel_tab = PANEL_PLAGUE;
    plague_fog_alpha = 50;
    scroll = independent_scroll_case(client, side_panel_w);
    pagination = pagination_case(client, side_panel_w);
    hitboxes = hitbox_case(client, side_panel_w);
    fixed_hover = fixed_hover_target_case(client, side_panel_w);
    pressed = pressed_state_case();
    ui_keys = cache_ui_key_case(client);
    data_keys = cache_data_key_case();
    ok = scroll && pagination && hitboxes && fixed_hover && pressed &&
         ui_keys && data_keys;
    fprintf(summary,
            "case=plague_panel_interaction ok=%d tab_state_persistence=%d independent_scroll=%d pagination_6_rows=%d paging_buttons=%d metric_selection=%d episode_selection=%d hover_hitboxes=%d fixed_hover_targets=%d pressed_release_state=%d cache_relevant_state=%d cache_ordinary_unchanged=%d cache_data_revisions=%d\n",
            ok, scroll, scroll, pagination, pagination, hitboxes, hitboxes,
            hitboxes, fixed_hover, pressed, ui_keys, ui_keys, data_keys);
    ui_language = old_language;
    side_panel_w = old_width;
    side_panel_collapsed = old_collapsed;
    panel_tab = old_panel;
    plague_fog_alpha = old_fog;
    ui_plague_panel_reset_presentation_state();
    return ok;
}
