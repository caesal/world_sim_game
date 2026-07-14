#include "ui/ui_panel_hover.h"

#include "core/game_types.h"
#include "render/panel_country.h"
#include "render/panel_country_diplomacy_tooltip.h"
#include "ui/ui_alliance_panel_input.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_layout.h"
#include "ui/ui_plague_fog.h"
#include "ui/ui_plague_panel.h"
#include "ui/ui_types.h"

static int last_panel_hover_target = -1;

enum {
    PANEL_HOVER_PLAGUE_FOG = 8000000,
    PANEL_HOVER_PLAGUE_PRIMARY_TAB_BASE = 8000100
};

static int plague_panel_hover_target(RECT client, int x, int y) {
    PlaguePanelLayout fog;
    int hit = ui_plague_panel_hit_test(client, side_panel_w, x, y);
    int i;
    if (hit != UI_PLAGUE_PANEL_HIT_NONE) {
        return panel_tab * 100000 + hit;
    }
    for (i = 0; i < PANEL_TAB_COUNT; i++) {
        if (point_in_rect(get_panel_tab_rect(client, i), x, y)) {
            return PANEL_HOVER_PLAGUE_PRIMARY_TAB_BASE + i;
        }
    }
    ui_plague_fog_layout_build(client, side_panel_w, &fog);
    if (point_in_rect(fog.slider.hit, x, y)) return PANEL_HOVER_PLAGUE_FOG;
    return panel_tab * 100000 + (x / 24) * 31 + y / 24;
}

int ui_panel_hover_target_key(RECT client, int x, int y) {
    int tooltip_key;
    if (side_panel_handle_hit_test(client, x, y)) return -2;
    if (!point_in_rect(get_side_panel_draw_rect(client), x, y)) return -1;
    if (side_panel_collapsed) return 1;
    if (panel_tab == PANEL_COUNTRY) {
        if (display_mode == DISPLAY_ALLIANCE && ui_alliance_panel_owns_input()) {
            return panel_tab * 100000 +
                   ui_alliance_panel_hover_hit(client, x, y);
        }
        if (selected_civ >= 0 &&
            country_detail_subtab == COUNTRY_DETAIL_DIPLOMACY) {
            tooltip_key = diplomacy_score_tooltip_hover_key_for_scope(
                SCORE_TOOLTIP_SCOPE_COUNTRY_DIPLOMACY, x, y);
            if (tooltip_key > 0) return 7000000 + tooltip_key;
        }
        return panel_tab * 100000 + country_panel_hit_test(client, x, y);
    }
    if (panel_tab == PANEL_PLAGUE) {
        return plague_panel_hover_target(client, x, y);
    }
    return panel_tab * 100000 + (x / 24) * 31 + y / 24;
}

static void invalidate_target(HWND hwnd, int old_target, int new_target) {
    if (old_target == -2 || new_target == -2) {
        ui_invalidate_side_panel_handle(hwnd);
    } else {
        ui_invalidate_side_panel_hover(hwnd);
    }
}

void ui_panel_hover_reset(void) {
    hover_x = -1;
    hover_y = -1;
    last_panel_hover_target = -1;
    ui_plague_panel_clear_hover();
}

void ui_panel_hover_update(HWND hwnd, RECT client, int old_x, int old_y,
                           int new_x, int new_y) {
    RECT panel_rect = get_side_panel_draw_rect(client);
    int was_panel = point_in_rect(panel_rect, old_x, old_y) ||
                    side_panel_handle_hit_test(client, old_x, old_y);
    int is_panel = point_in_rect(panel_rect, new_x, new_y) ||
                   side_panel_handle_hit_test(client, new_x, new_y);
    int new_target = ui_panel_hover_target_key(client, new_x, new_y);

    if (panel_tab == PANEL_PLAGUE && is_panel) {
        ui_plague_panel_set_hover_target(
            ui_plague_panel_hit_test(client, side_panel_w, new_x, new_y));
    } else {
        ui_plague_panel_clear_hover();
    }
    if ((panel_tab == PANEL_COUNTRY || panel_tab == PANEL_POPULATION ||
         panel_tab == PANEL_PLAGUE || panel_tab == PANEL_WORLD ||
         panel_tab == PANEL_DEBUG) &&
        (was_panel || is_panel) && new_target != last_panel_hover_target) {
        int old_target = last_panel_hover_target;
        last_panel_hover_target = new_target;
        invalidate_target(hwnd, old_target, new_target);
    }
}

void ui_panel_hover_leave(HWND hwnd, RECT client) {
    int old_target = last_panel_hover_target;
    last_panel_hover_target = -1;
    ui_plague_panel_clear_hover();
    if (old_target != -1) {
        invalidate_target(hwnd, old_target,
                          ui_panel_hover_target_key(client, -1, -1));
    }
}
