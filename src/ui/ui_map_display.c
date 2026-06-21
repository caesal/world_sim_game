#include "ui/ui_map_display.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "game/game_loop.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_layout.h"
#include "ui/ui_types.h"

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
    int new_mode;
    if (mode_index < 0 || mode_index >= MAP_DISPLAY_MODE_COUNT) return 0;
    new_mode = MAP_DISPLAY_MODES[mode_index];
    if (display_mode == new_mode) return 1;
    display_mode = new_mode;
    map_interaction_preview = 0;
    dirty_mark_labels();
    ui_invalidate_game_redraw(hwnd, GAME_REDRAW_MAP_STATIC | GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_SIDE_PANEL);
    return 1;
}

int ui_handle_top_map_display_click(HWND hwnd, RECT client, int mouse_x, int mouse_y) {
    int i;
    for (i = 0; i < MAP_DISPLAY_MODE_COUNT; i++) {
        if (point_in_rect(get_mode_button_rect(client, i), mouse_x, mouse_y)) {
            return ui_set_map_display_mode(hwnd, i);
        }
    }
    return 0;
}
