#include "ui/ui_map_display.h"

#include "core/constants.h"
#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "game/game_loop.h"
#include "render/map_label_cache_key.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_layout.h"
#include "ui/ui_types.h"
#include "ui/ui_pressed_state.h"
#include "ui/ui_wheel.h"

static int pending_mode_switch_redraw_flags;

static int mode_switch_side_panel_depends(int old_mode, int new_mode) {
    return old_mode == DISPLAY_ALLIANCE || new_mode == DISPLAY_ALLIANCE;
}

static void request_next_frame_map_mode_redraw(HWND hwnd, int redraw_flags) {
    if (!hwnd) return;
    pending_mode_switch_redraw_flags |= redraw_flags;
    KillTimer(hwnd, MAP_PREVIEW_TIMER_ID);
    SetTimer(hwnd, MAP_PREVIEW_TIMER_ID, FRAME_TIMER_MS, NULL);
}

int ui_map_display_handle_preview_timer(HWND hwnd) {
    int redraw_flags = pending_mode_switch_redraw_flags;
    pending_mode_switch_redraw_flags = 0;
    map_interaction_preview = 0;
    KillTimer(hwnd, MAP_PREVIEW_TIMER_ID);
    ui_wheel_invalidate_map_viewport(hwnd);
    if (redraw_flags) ui_invalidate_game_redraw(hwnd, redraw_flags);
    return 1;
}

const char *ui_map_display_label(int mode_index, int language) {
    static const char *names_en[MAP_DISPLAY_MODE_COUNT] = {
        "Country", "Alliance", "Geography", "Climate", "Province", "Routes"
    };
    static const char *names_zh[MAP_DISPLAY_MODE_COUNT] = {
        "国家", "联盟", "地理", "气候", "行省", "航道"
    };
    mode_index = clamp(mode_index, 0, MAP_DISPLAY_MODE_COUNT - 1);
    return language == UI_LANG_ZH ? names_zh[mode_index] : names_en[mode_index];
}

const char *ui_primary_panel_tab_label(int language) {
    return display_mode == DISPLAY_ALLIANCE ?
           (language == UI_LANG_ZH ? "联盟" : "Alliance") :
           (language == UI_LANG_ZH ? "国家" : "Country");
}

int ui_set_map_display_mode(HWND hwnd, int mode_index) {
    int old_mode, new_mode;
    int followup_flags = GAME_REDRAW_SIDE_PANEL_DATA;
    if (mode_index < 0 || mode_index >= MAP_DISPLAY_MODE_COUNT) return 0;
    new_mode = MAP_DISPLAY_MODES[mode_index];
    if (display_mode == new_mode) return 1;
    old_mode = display_mode;
    display_mode = new_mode;
    map_interaction_preview = 0;
    if (map_label_cache_display_family_changed(old_mode, new_mode)) dirty_mark_labels();
    if (mode_switch_side_panel_depends(old_mode, new_mode)) followup_flags |= GAME_REDRAW_SIDE_PANEL;
    ui_invalidate_game_redraw(hwnd, GAME_REDRAW_TOP_BAR);
    request_next_frame_map_mode_redraw(hwnd, followup_flags);
    return 1;
}

int ui_handle_top_map_display_click(HWND hwnd, RECT client, int mouse_x, int mouse_y) {
    int i;
    for (i = 0; i < MAP_DISPLAY_MODE_COUNT; i++) {
        if (point_in_rect(get_mode_button_rect(client, i), mouse_x, mouse_y)) {
            ui_pressed_control_set(hwnd, UI_PRESSED_MAP_MODE, i);
            return ui_set_map_display_mode(hwnd, i);
        }
    }
    return 0;
}
