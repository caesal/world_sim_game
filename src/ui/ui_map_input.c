#include "ui/ui_map_input.h"

#include "core/game_types.h"
#include "render/render.h"
#include "ui/ui_layout.h"
#include "ui/ui_selection.h"
#include "ui/ui_snapshot_read.h"

static int selected_tile_owner(void) {
    return ui_snapshot_tile_owner(selected_x, selected_y);
}

void ui_select_tile_from_mouse(HWND hwnd, int mouse_x, int mouse_y) {
    RECT client;
    MapLayout layout;
    int x;
    int y;
    int owner;
    if (!world_generated) return;
    GetClientRect(hwnd, &client);
    layout = get_map_layout(client);
    if (mouse_x < layout.map_x || mouse_y < layout.map_y ||
        mouse_x >= layout.map_x + layout.draw_w || mouse_y >= layout.map_y + layout.draw_h) {
        return;
    }
    x = (mouse_x - layout.map_x) * MAP_W / layout.draw_w;
    y = (mouse_y - layout.map_y) * MAP_H / layout.draw_h;
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return;
    selected_x = x;
    selected_y = y;
    owner = selected_tile_owner();
    if (ui_snapshot_civ_alive(owner)) ui_select_civ_preserve_view(owner, UI_SELECT_SOURCE_MAP);
    else ui_clear_selected_civ(UI_SELECT_SOURCE_MAP);
    InvalidateRect(hwnd, NULL, FALSE);
}
