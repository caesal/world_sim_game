#include "ui/ui_pressed_state.h"

#include "ui/ui_invalidation.h"

static UiPressedControlKind pressed_kind = UI_PRESSED_NONE;
static int pressed_index = -1;

static void invalidate_pressed(HWND hwnd, UiPressedControlKind kind) {
    if (!hwnd) return;
    if (kind == UI_PRESSED_MAP_MODE) ui_invalidate_top_bar(hwnd);
    else if (kind == UI_PRESSED_PLAY || kind == UI_PRESSED_SPEED) ui_invalidate_bottom_bar(hwnd);
}

void ui_pressed_control_set(HWND hwnd, UiPressedControlKind kind, int index) {
    UiPressedControlKind old = pressed_kind;
    pressed_kind = kind;
    pressed_index = index;
    invalidate_pressed(hwnd, old);
    invalidate_pressed(hwnd, kind);
    if (hwnd) UpdateWindow(hwnd);
}

void ui_pressed_control_clear(HWND hwnd) {
    UiPressedControlKind old = pressed_kind;
    if (pressed_kind == UI_PRESSED_NONE) return;
    pressed_kind = UI_PRESSED_NONE;
    pressed_index = -1;
    invalidate_pressed(hwnd, old);
}

int ui_pressed_control_is_active(UiPressedControlKind kind, int index) {
    return pressed_kind == kind && pressed_index == index;
}
