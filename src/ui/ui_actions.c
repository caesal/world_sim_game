#include "ui/ui_actions.h"

#include "core/game_state.h"
#include "game/game.h"
#include "io/map_save.h"
#include "ui/pause_menu.h"
#include "ui/ui_forms.h"
#include "ui/ui_worldgen_command.h"

void ui_set_speed(int index) { speed_index = clamp(index, 0, SPEED_COUNT - 1); }

void ui_handle_pause_menu_action(HWND hwnd, int hit) {
    if (hit == PAUSE_MENU_RESUME_GAME) {
        pause_menu_open = 0;
    } else if (hit == PAUSE_MENU_VERSION_LOG) {
        pause_menu_show_version_log(hwnd);
    } else if (hit == PAUSE_MENU_SAVE_MAP) {
        if (game_pause_in_progress()) return;
        save_current_map(hwnd);
    } else if (hit == PAUSE_MENU_LOAD_MAP) {
        if (game_pause_in_progress()) return;
        if (load_map_from_file(hwnd)) {
            ui_worldgen_command_resync_after_load(hwnd);
        }
    } else if (hit == PAUSE_MENU_EXIT_GAME) {
        DestroyWindow(hwnd);
    }
}
