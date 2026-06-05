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

void ui_clay_draw_section_header(HDC hdc, RECT rect, const char *label) {
    UiClayStyle style = ui_clay_style(UI_CLAY_SURFACE_CARD, UI_CLAY_STATE_NORMAL);
    RECT text = rect;
    RECT rule = rect;

    if (rect.right <= rect.left || rect.bottom <= rect.top) return;
    InflateRect(&text, -2, 0);
    draw_text_rect(hdc, text, label, style.text,
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    rule.top = rect.bottom - 2;
    rule.bottom = rule.top + 1;
    fill_rect(hdc, rule, style.border);
    rule.top = rect.bottom - 1;
    rule.bottom = rect.bottom;
    fill_rect(hdc, rule, style.highlight);
}

void ui_clay_draw_input_frame(HDC hdc, RECT rect, UiClayState state) {
    RECT inner = rect;

    if (rect.right <= rect.left || rect.bottom <= rect.top) return;
    ui_clay_draw_card(hdc, rect, state);
    InflateRect(&inner, -3, -3);
    if (inner.right > inner.left && inner.bottom > inner.top) {
        fill_rect(hdc, inner, RGB(31, 38, 42));
    }
}

void ui_clay_draw_slider(HDC hdc, RECT track, int value, UiClayState state) {
    RECT inner = track;
    RECT fill;
    RECT knob;
    int width;
    int knob_x;

    if (track.right <= track.left || track.bottom <= track.top) return;
    value = clamp(value, 0, 100);
    width = track.right - track.left;
    knob_x = track.left + width * value / 100;
    ui_clay_draw_pill_inset(hdc, track, UI_CLAY_STATE_NORMAL);
    InflateRect(&inner, -3, -3);
    if (inner.right > inner.left && inner.bottom > inner.top) {
        fill_rect(hdc, inner, RGB(47, 58, 63));
        fill = inner;
        fill.right = clamp(knob_x, inner.left, inner.right);
        if (fill.right > fill.left) fill_rect(hdc, fill, RGB(86, 146, 176));
    }
    knob = (RECT){knob_x - 8, track.top - 6, knob_x + 8, track.bottom + 6};
    ui_clay_draw_pill(hdc, knob, state);
}

void ui_clay_draw_swatch(HDC hdc, RECT rect, COLORREF color, UiClayState state) {
    RECT inner = rect;

    if (rect.right <= rect.left || rect.bottom <= rect.top) return;
    ui_clay_draw_card(hdc, rect, state);
    InflateRect(&inner, -5, -5);
    if (inner.right > inner.left && inner.bottom > inner.top) {
        fill_rect(hdc, inner, color);
    }
}
