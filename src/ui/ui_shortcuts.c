#include "ui/ui.h"

#include "game/game.h"
#include "game/game_loop.h"
#include "ui/color_picker.h"
#include "ui/pause_menu.h"
#include "ui/ui_country_target.h"
#include "ui/ui_forms.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_worldgen_command.h"
#include "ui/ui_worldgen_input.h"

int handle_shortcut(HWND hwnd, WPARAM key) {
    if (ui_worldgen_input_key_down(hwnd, key)) return 1;
    if (ui_country_target_active()) {
        if (key == VK_ESCAPE) return ui_country_target_cancel(hwnd);
        return 1;
    }
    if (key == VK_ESCAPE) {
        game_pause_for_modal_or_action();
        if (color_picker_active()) {
            color_picker_close();
            ui_invalidate_full(hwnd);
            return 1;
        }
        pause_menu_open = !pause_menu_open;
        ui_forms_layout(hwnd);
        ui_invalidate_full(hwnd);
        return 1;
    }
    if (color_picker_active() || pause_menu_open) return 1;
    if (key == VK_SPACE) {
        game_toggle_auto_run();
        ui_invalidate_game_redraw(
            hwnd, GAME_REDRAW_TOP_BAR | GAME_REDRAW_BOTTOM_BAR |
                  (panel_tab == PANEL_WORLD ? GAME_REDRAW_SIDE_PANEL : 0));
        return 1;
    }
    if (key == VK_F1) {
        ui_forms_add_civ(hwnd);
        return 1;
    }
    if (key == VK_F2) {
        ui_forms_apply_selected(hwnd);
        return 1;
    }
    if (key == VK_F5 || key == 'R') {
        ui_worldgen_command_generate(hwnd);
        return 1;
    }
    return 0;
}

int is_game_shortcut(WPARAM key) {
    return key == VK_SPACE || key == VK_F1 || key == VK_F2 ||
           key == VK_F5 || key == VK_ESCAPE || key == VK_RETURN;
}

int is_game_char_shortcut(WPARAM key) { return key == ' '; }

int handle_char_shortcut(HWND hwnd, WPARAM key) {
    if (ui_country_target_active() || color_picker_active() || pause_menu_open)
        return 1;
    if (key == ' ') return handle_shortcut(hwnd, VK_SPACE);
    return 0;
}
