#include "ui/ui_map_input.h"

#include "core/game_types.h"
#include "render/render.h"
#include "game/game_loop.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_layout.h"
#include "ui/ui_selection.h"
#include "ui/ui_snapshot_read.h"

static int selected_tile_owner(void) {
    return ui_snapshot_tile_owner(selected_x, selected_y);
}

int ui_map_screen_to_tile(HWND hwnd, int mouse_x, int mouse_y, int *out_x, int *out_y) {
    RECT client;
    MapLayout layout;
    int x;
    int y;
    if (!world_generated || !out_x || !out_y) return 0;
    GetClientRect(hwnd, &client);
    layout = get_map_layout(client);
    if (mouse_x < layout.map_x || mouse_y < layout.map_y ||
        mouse_x >= layout.map_x + layout.draw_w || mouse_y >= layout.map_y + layout.draw_h) {
        return 0;
    }
    x = (mouse_x - layout.map_x) * MAP_W / layout.draw_w;
    y = (mouse_y - layout.map_y) * MAP_H / layout.draw_h;
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return 0;
    *out_x = x;
    *out_y = y;
    return 1;
}

void ui_select_tile_from_mouse(HWND hwnd, int mouse_x, int mouse_y) {
    int x;
    int y;
    int owner;
    if (!ui_map_screen_to_tile(hwnd, mouse_x, mouse_y, &x, &y)) return;
    selected_x = x;
    selected_y = y;
    owner = selected_tile_owner();
    if (ui_snapshot_civ_alive(owner)) {
        int alliance_id = display_mode == DISPLAY_ALLIANCE ?
                          ui_snapshot_civ_alliance_display(owner) : -1;
        if (alliance_id >= 0) {
            int keep_tab = selected_alliance_id >= 0;
            int tab = keep_tab ? clamp(alliance_detail_subtab, 0, ALLIANCE_DETAIL_TAB_COUNT - 1) :
                      ALLIANCE_DETAIL_OVERVIEW;
            ui_select_civ_preserve_view(owner, UI_SELECT_SOURCE_MAP);
            selected_alliance_id = alliance_id;
            alliance_detail_subtab = tab;
            alliance_detail_scroll_offsets[tab] = 0;
            alliance_detail_scroll_offset = 0;
        } else {
            ui_select_civ_preserve_view(owner, UI_SELECT_SOURCE_MAP);
        }
    } else ui_clear_selected_civ(UI_SELECT_SOURCE_MAP);
    ui_invalidate_game_redraw(hwnd, GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_SIDE_PANEL);
}
