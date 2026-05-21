#include "platform/sdl_country_input.h"

#include "core/country_focus.h"
#include "core/game_state.h"
#include "core/render_snapshot.h"
#include "game/game.h"
#include "render/panel_country.h"
#include "render/panel_country_diplomacy_hits.h"
#include "render/panel_country_events.h"
#include "ui/color_picker.h"
#include "ui/ui_types.h"

static int snapshot_civ_alive_id(int civ_id) {
    const RenderSnapshot *snapshot = render_snapshot_acquire();
    int alive = snapshot && civ_id >= 0 && civ_id < snapshot->civ_count &&
                snapshot->civs[civ_id].alive;
    render_snapshot_release(snapshot);
    return alive;
}

static void select_civ_list(int civ_id, int preserve_detail) {
    if (civ_id < 0 || civ_id >= civ_count) return;
    if (selected_civ != civ_id) previous_selected_civ = selected_civ;
    selected_civ = civ_id;
    map_highlight_civ = -1;
    selected_civ_pulse_start_ms = (int)GetTickCount();
    map_highlight_pulse_start_ms = 0;
    if (!preserve_detail) country_detail_subtab = COUNTRY_DETAIL_OVERVIEW;
    country_detail_scroll_offsets[clamp(country_detail_subtab, 0, COUNTRY_DETAIL_TAB_COUNT - 1)] = 0;
    country_detail_scroll_offset = 0;
}

static void clear_selected_civ_local(void) {
    selected_civ = -1;
    previous_selected_civ = -1;
    map_highlight_civ = -1;
    selected_civ_pulse_start_ms = 0;
    map_highlight_pulse_start_ms = 0;
    country_detail_scroll_offset = 0;
}

static void highlight_civ_local(int civ_id) {
    if (civ_id < 0 || civ_id >= civ_count) return;
    map_highlight_civ = civ_id;
    map_highlight_pulse_start_ms = (int)GetTickCount();
}

static void open_civ_color_picker(void) {
    const RenderSnapshot *snapshot;
    if (selected_civ < 0) return;
    snapshot = render_snapshot_acquire();
    if (snapshot && selected_civ < snapshot->civ_count) {
        color_picker_open_civ(selected_civ, snapshot->civs[selected_civ].color);
    }
    render_snapshot_release(snapshot);
}

int sdl_country_panel_click(RECT client, int x, int y) {
    int hit;
    if (side_panel_collapsed || panel_tab != PANEL_COUNTRY || x < client.right - side_panel_w) return 0;
    hit = country_panel_hit_test(client, x, y);
    if (selected_civ >= 0 && country_detail_subtab == COUNTRY_DETAIL_OVERVIEW) {
        int event_civ = country_recent_events_country_hit_test(x, y);
        if (event_civ >= 0) {
            highlight_civ_local(event_civ);
            return 1;
        }
    }
    if (selected_civ >= 0 && country_detail_subtab == COUNTRY_DETAIL_DIPLOMACY) {
        int relation_civ = country_diplomacy_civ_hit_test(x, y);
        if (snapshot_civ_alive_id(relation_civ) && relation_civ != selected_civ) {
            select_civ_list(relation_civ, 1);
            return 1;
        }
    }
    if (hit == COUNTRY_PANEL_HIT_TOGGLE_FALLEN) {
        country_show_fallen = !country_show_fallen;
        country_list_scroll_offset = 0;
        if (!country_show_fallen && selected_civ >= 0 && !snapshot_civ_alive_id(selected_civ)) {
            clear_selected_civ_local();
        }
        return 1;
    }
    if (hit == COUNTRY_PANEL_HIT_BACK_TO_LIST) {
        clear_selected_civ_local();
        country_list_scroll_offset = 0;
        country_detail_scroll_offset = 0;
        return 1;
    }
    if (hit == COUNTRY_PANEL_HIT_CIVIL_UNREST) {
        game_pause_for_modal_or_action();
        game_request_trigger_civil_unrest(selected_civ);
        country_detail_scroll_offsets[clamp(country_detail_subtab, 0, COUNTRY_DETAIL_TAB_COUNT - 1)] = 0;
        return 1;
    }
    if (hit == COUNTRY_PANEL_HIT_VASSAL_ACTION) {
        int vassal_id = country_panel_vassal_action_target(client, x, y);
        game_pause_for_modal_or_action();
        if (vassal_id >= 0) game_request_release_vassal(vassal_id);
        return 1;
    }
    if (hit == COUNTRY_PANEL_HIT_COLOR) {
        open_civ_color_picker();
        return 1;
    }
    if (hit == COUNTRY_PANEL_HIT_LOCATE) {
        int fx;
        int fy;
        if (country_focus_point(selected_civ, &fx, &fy)) {
            selected_x = fx;
            selected_y = fy;
            highlight_civ_local(selected_civ);
        }
        return 1;
    }
    if (hit <= COUNTRY_PANEL_HIT_SUBTAB_BASE &&
        hit > COUNTRY_PANEL_HIT_SUBTAB_BASE - COUNTRY_DETAIL_TAB_COUNT) {
        country_detail_subtab = clamp(COUNTRY_PANEL_HIT_SUBTAB_BASE - hit, 0, COUNTRY_DETAIL_TAB_COUNT - 1);
        return 1;
    }
    if (hit <= COUNTRY_PANEL_HIT_DIPLOMACY_VIEW_BASE &&
        hit >= COUNTRY_PANEL_HIT_DIPLOMACY_VIEW_BASE - DIPLOMACY_VIEW_OTHER) {
        country_diplomacy_view = COUNTRY_PANEL_HIT_DIPLOMACY_VIEW_BASE - hit;
        country_detail_scroll_offsets[COUNTRY_DETAIL_DIPLOMACY] = 0;
        return 1;
    }
    if (hit <= COUNTRY_PANEL_HIT_SORT_POPULATION && hit >= COUNTRY_PANEL_HIT_SORT_DISORDER) {
        int column = COUNTRY_PANEL_HIT_SORT_POPULATION - hit;
        if (country_sort_column == column) country_sort_descending = !country_sort_descending;
        else {
            country_sort_column = column;
            country_sort_descending = 1;
        }
        country_list_scroll_offset = 0;
        return 1;
    }
    if (hit >= 0 && snapshot_civ_alive_id(hit)) {
        select_civ_list(hit, 0);
        return 1;
    }
    return x >= client.right - side_panel_w;
}
