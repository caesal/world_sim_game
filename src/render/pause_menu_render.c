#include "pause_menu_render.h"

#include "core/version.h"
#include "render/render_common.h"
#include "ui/pause_menu.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_theme.h"

static void draw_menu_button(HDC hdc, RECT rect, const char *label) {
    ui_clay_draw_pill_button(hdc, rect, label,
                             ui_clay_state_for_rect(rect, hover_x, hover_y, 0, 0));
}

void draw_pause_menu_overlay(HDC hdc, RECT client) {
    RECT panel = pause_menu_panel_rect(client);
    RECT title = panel;
    RECT subtitle = panel;
    int i;

    fill_rect_alpha(hdc, client, RGB(8, 10, 12), 168);
    ui_clay_draw_menu_panel(hdc, panel);
    title.top += 22;
    title.bottom = title.top + 28;
    draw_center_text(hdc, title, "World Sim Game", ui_theme_color(UI_COLOR_TEXT));
    subtitle.top = title.bottom + 2;
    subtitle.bottom = subtitle.top + 22;
    draw_center_text(hdc, subtitle, WORLD_SIM_VERSION_LABEL, ui_theme_color(UI_COLOR_TEXT_MUTED));
    for (i = 0; i < PAUSE_MENU_BUTTON_COUNT; i++) {
        draw_menu_button(hdc, pause_menu_button_rect(client, i), pause_menu_button_label(i));
    }
}
