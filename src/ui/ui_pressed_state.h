#ifndef WORLD_SIM_UI_PRESSED_STATE_H
#define WORLD_SIM_UI_PRESSED_STATE_H

#include <windows.h>

typedef enum {
    UI_PRESSED_NONE = 0,
    UI_PRESSED_PLAY,
    UI_PRESSED_SPEED,
    UI_PRESSED_MAP_MODE
} UiPressedControlKind;

void ui_pressed_control_set(HWND hwnd, UiPressedControlKind kind, int index);
void ui_pressed_control_clear(HWND hwnd);
int ui_pressed_control_is_active(UiPressedControlKind kind, int index);

#endif
