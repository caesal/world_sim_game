#include "ui/ui_map_display.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "game/game_loop.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_types.h"

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
