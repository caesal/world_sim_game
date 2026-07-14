#include "ui/ui_plague_input.h"

#include "ui/ui_invalidation.h"
#include "ui/ui_plague_panel.h"
#include "ui/ui_plague_panel_layout.h"
#include "ui/ui_plague_probability.h"
#include "ui/ui_pressed_state.h"

int ui_plague_input_mouse_down(HWND hwnd, RECT client, int panel_width,
                               int x, int y) {
    UiPlaguePanelLayout layout;
    int hit;
    ui_plague_panel_layout_build(client, panel_width, &layout);
    if (ui_plague_probability_mouse_down(hwnd, &layout.probability, x, y)) {
        return 1;
    }
    hit = ui_plague_panel_hit_test(client, panel_width, x, y);
    if (hit >= UI_PLAGUE_PANEL_HIT_MAIN_TAB_BASE &&
        hit < UI_PLAGUE_PANEL_HIT_MAIN_TAB_BASE + PLAGUE_PANEL_TAB_COUNT) {
        ui_pressed_control_set(hwnd, UI_PRESSED_PLAGUE_MAIN_TAB,
                               hit - UI_PLAGUE_PANEL_HIT_MAIN_TAB_BASE);
    } else if (hit == UI_PLAGUE_PANEL_HIT_IMPACT_PREVIOUS ||
               hit == UI_PLAGUE_PANEL_HIT_IMPACT_NEXT) {
        ui_pressed_control_set(hwnd, UI_PRESSED_PLAGUE_IMPACT_PAGER,
                               hit - UI_PLAGUE_PANEL_HIT_IMPACT_PREVIOUS);
    } else if (hit >= UI_PLAGUE_PANEL_HIT_HISTORY_METRIC_BASE &&
               hit < UI_PLAGUE_PANEL_HIT_HISTORY_METRIC_BASE +
                     PLAGUE_HISTORY_METRIC_COUNT) {
        ui_pressed_control_set(hwnd, UI_PRESSED_PLAGUE_HISTORY_METRIC,
                               hit - UI_PLAGUE_PANEL_HIT_HISTORY_METRIC_BASE);
    }
    if (!ui_plague_panel_handle_click(client, panel_width, x, y)) return 0;
    ui_invalidate_side_panel(hwnd);
    return 1;
}

int ui_plague_input_mouse_move(HWND hwnd, RECT client, int panel_width,
                               int x, int y) {
    UiPlaguePanelLayout layout;
    ui_plague_panel_layout_build(client, panel_width, &layout);
    return ui_plague_probability_mouse_move(hwnd, &layout.probability, x, y);
}

int ui_plague_input_mouse_up(HWND hwnd, RECT client, int panel_width,
                             int x, int y) {
    UiPlaguePanelLayout layout;
    ui_plague_panel_layout_build(client, panel_width, &layout);
    return ui_plague_probability_mouse_up(hwnd, &layout.probability, x, y);
}

void ui_plague_input_panel_closed(void) {
    ui_plague_probability_panel_closed();
}
