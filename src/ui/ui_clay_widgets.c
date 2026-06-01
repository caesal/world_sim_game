#include "ui/ui_clay_widgets.h"

#include "render/render_common.h"
#include "ui/ui_clay_primitives.h"

UiClayState ui_clay_state_from_flags(int hovered, int pressed, int selected, int disabled) {
    if (disabled) return UI_CLAY_STATE_DISABLED;
    if (pressed) return UI_CLAY_STATE_PRESSED;
    if (selected) return UI_CLAY_STATE_SELECTED;
    if (hovered) return UI_CLAY_STATE_HOVER;
    return UI_CLAY_STATE_NORMAL;
}

UiClayState ui_clay_state_for_rect(RECT rect, int x, int y, int selected, int disabled) {
    int hovered = point_in_rect_local(rect, x, y);
    return ui_clay_state_from_flags(hovered, hovered && (GetKeyState(VK_LBUTTON) & 0x8000),
                                    selected, disabled);
}

static void clay_draw_label(HDC hdc, RECT rect, const char *label, UiClayState state) {
    RECT text_rect = rect;

    InflateRect(&text_rect, -3, -1);
    draw_center_text(hdc, text_rect, label, ui_clay_text_color(state));
}

void ui_clay_draw_button(HDC hdc, RECT rect, const char *label, UiClayState state) {
    ui_clay_draw_card(hdc, rect, state);
    clay_draw_label(hdc, rect, label, state);
}

void ui_clay_draw_pill_button(HDC hdc, RECT rect, const char *label, UiClayState state) {
    ui_clay_draw_pill(hdc, rect, state);
    clay_draw_label(hdc, rect, label, state);
}

void ui_clay_draw_icon_button(HDC hdc, RECT rect, const char *label, UiClayState state) {
    ui_clay_draw_pill_inset(hdc, rect, state);
    clay_draw_label(hdc, rect, label, state);
}

void ui_clay_draw_menu_panel(HDC hdc, RECT rect) {
    ui_clay_draw_panel(hdc, rect, UI_CLAY_STATE_NORMAL);
}
