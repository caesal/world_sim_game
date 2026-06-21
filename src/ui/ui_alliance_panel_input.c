#include "ui/ui_alliance_panel_input.h"

#include "game/game_loop.h"
#include "render/panel_alliance.h"
#include "render/panel_alliance_model.h"
#include "render/render_context.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_selection.h"
#include "ui/ui_snapshot_read.h"
#include "ui/ui_types.h"

static const AlliancePanelRow *selected_row_from_snapshot(const RenderSnapshot *snapshot,
                                                          int alliance_id) {
    const AlliancePanelModel *model = alliance_panel_model_get(snapshot, country_show_fallen,
                                                               country_sort_column, country_sort_descending);
    return alliance_panel_model_find_alliance(model, alliance_id);
}

static void open_alliance_detail(HWND hwnd, const RenderSnapshot *snapshot, int alliance_id) {
    const AlliancePanelRow *row = selected_row_from_snapshot(snapshot, alliance_id);
    int keep_tab = selected_alliance_id >= 0;
    int tab = keep_tab ? clamp(alliance_detail_subtab, 0, ALLIANCE_DETAIL_TAB_COUNT - 1) :
              ALLIANCE_DETAIL_OVERVIEW;
    if (row && row->leader_civ >= 0) ui_select_civ_preserve_view(row->leader_civ,
                                                                 UI_SELECT_SOURCE_ALLIANCE_ROW);
    selected_alliance_id = alliance_id;
    alliance_detail_subtab = tab;
    alliance_detail_scroll_offset = 0;
    alliance_detail_scroll_offsets[tab] = 0;
    ui_invalidate_game_redraw(hwnd, GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_SIDE_PANEL);
}

int ui_alliance_panel_owns_input(void) {
    if (display_mode != DISPLAY_ALLIANCE || panel_tab != PANEL_COUNTRY) return 0;
    if (selected_alliance_id >= 0 || selected_civ < 0) return 1;
    return ui_snapshot_civ_alliance_display(selected_civ) >= 0;
}

int ui_alliance_panel_hover_hit(RECT client, int mouse_x, int mouse_y) {
    return alliance_panel_hit_test(client, mouse_x, mouse_y);
}

int ui_handle_alliance_panel_click(HWND hwnd, RECT client, int mouse_x, int mouse_y) {
    int hit;
    if (!ui_alliance_panel_owns_input() || side_panel_collapsed ||
        mouse_x < client.right - side_panel_w) return 0;
    hit = alliance_panel_hit_test(client, mouse_x, mouse_y);
    if (hit == ALLIANCE_PANEL_HIT_NONE) return 0;
    if (hit == ALLIANCE_PANEL_HIT_TOGGLE_FALLEN) {
        country_show_fallen = !country_show_fallen;
        alliance_list_scroll_offset = 0;
        selected_alliance_id = -1;
        ui_invalidate_side_panel(hwnd);
        return 1;
    }
    if (hit <= ALLIANCE_PANEL_HIT_SORT_POPULATION && hit >= ALLIANCE_PANEL_HIT_SORT_DISORDER) {
        int column = ALLIANCE_PANEL_HIT_SORT_POPULATION - hit;
        if (country_sort_column == column) country_sort_descending = !country_sort_descending;
        else { country_sort_column = column; country_sort_descending = 1; }
        alliance_list_scroll_offset = 0;
        ui_invalidate_side_panel(hwnd);
        return 1;
    }
    if (hit == ALLIANCE_PANEL_HIT_BACK_TO_LIST) {
        selected_alliance_id = -1;
        alliance_list_scroll_offset = 0;
        ui_invalidate_game_redraw(hwnd, GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_SIDE_PANEL);
        return 1;
    }
    if (hit <= ALLIANCE_PANEL_HIT_SUBTAB_BASE &&
        hit > ALLIANCE_PANEL_HIT_SUBTAB_BASE - ALLIANCE_DETAIL_TAB_COUNT) {
        int tab = clamp(ALLIANCE_PANEL_HIT_SUBTAB_BASE - hit, 0, ALLIANCE_DETAIL_TAB_COUNT - 1);
        if (alliance_detail_subtab != tab) {
            alliance_detail_subtab = tab;
            alliance_detail_scroll_offsets[tab] = 0;
            alliance_detail_scroll_offset = 0;
            ui_invalidate_side_panel(hwnd);
        }
        return 1;
    }
    if (hit <= ALLIANCE_PANEL_HIT_MEMBER_SORT_BASE &&
        hit > ALLIANCE_PANEL_HIT_MEMBER_SORT_BASE - ALLIANCE_MEMBER_SORT_COUNT) {
        int column = clamp(ALLIANCE_PANEL_HIT_MEMBER_SORT_BASE - hit, 0,
                           ALLIANCE_MEMBER_SORT_COUNT - 1);
        if (alliance_member_sort_column != column) {
            alliance_member_sort_column = column;
            alliance_detail_scroll_offsets[ALLIANCE_DETAIL_MEMBERS] = 0;
            alliance_detail_scroll_offset = 0;
            ui_invalidate_side_panel(hwnd);
        }
        return 1;
    }
    if (hit >= ALLIANCE_PANEL_HIT_ALLIANCE_BASE &&
        hit < ALLIANCE_PANEL_HIT_ALLIANCE_BASE + ALLIANCE_MAX) {
        int owned = 0;
        int alliance_id = hit - ALLIANCE_PANEL_HIT_ALLIANCE_BASE;
        const RenderSnapshot *snapshot = render_context_snapshot();
        if (!snapshot) { snapshot = render_snapshot_acquire(); render_context_begin(snapshot); owned = 1; }
        open_alliance_detail(hwnd, snapshot, alliance_id);
        if (owned) { render_context_end(); render_snapshot_release(snapshot); }
        return 1;
    }
    if (hit >= ALLIANCE_PANEL_HIT_COUNTRY_BASE &&
        hit < ALLIANCE_PANEL_HIT_COUNTRY_BASE + MAX_CIVS) {
        int owned = 0;
        int civ_id = hit - ALLIANCE_PANEL_HIT_COUNTRY_BASE;
        const RenderSnapshot *snapshot = render_context_snapshot();
        int alliance_id = -1;
        if (!snapshot) { snapshot = render_snapshot_acquire(); render_context_begin(snapshot); owned = 1; }
        if (snapshot && civ_id >= 0 && civ_id < snapshot->civ_count && snapshot->civs[civ_id].alive)
            alliance_id = snapshot->civs[civ_id].alliance_display_id;
        if (alliance_id >= 0) open_alliance_detail(hwnd, snapshot, alliance_id);
        else {
            ui_select_civ_preserve_view(civ_id, UI_SELECT_SOURCE_COUNTRY_LIST);
            selected_alliance_id = -1;
            ui_invalidate_game_redraw(hwnd, GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_SIDE_PANEL);
        }
        if (owned) { render_context_end(); render_snapshot_release(snapshot); }
        return 1;
    }
    return 1;
}
