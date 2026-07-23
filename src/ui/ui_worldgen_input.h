#ifndef WORLD_SIM_UI_WORLDGEN_INPUT_H
#define WORLD_SIM_UI_WORLDGEN_INPUT_H

#include <windows.h>

typedef struct {
    unsigned int mouse_downs;
    unsigned int mouse_moves;
    unsigned int mouse_ups;
    unsigned int value_writes;
    unsigned int drag_updates;
    unsigned int release_outside_cancels;
    unsigned int wheel_changes;
} UiWorldgenInputDiagnostics;

int ui_worldgen_input_mouse_down(HWND hwnd, RECT client, int panel_width,
                                 int mouse_x, int mouse_y);
int ui_worldgen_input_owns_panel_point(RECT client, int panel_width,
                                       int mouse_x, int mouse_y);
int ui_worldgen_input_mouse_move(HWND hwnd, RECT client, int panel_width,
                                 int mouse_x, int mouse_y);
int ui_worldgen_input_mouse_up(HWND hwnd, RECT client, int panel_width,
                               int mouse_x, int mouse_y);
int ui_worldgen_input_mouse_leave(HWND hwnd);
int ui_worldgen_input_wheel(HWND hwnd, RECT client, int panel_width,
                            int mouse_x, int mouse_y, int steps);
int ui_worldgen_input_key_down(HWND hwnd, WPARAM key);

void ui_worldgen_input_get_diagnostics(UiWorldgenInputDiagnostics *out);
void ui_worldgen_input_reset_diagnostics(void);

#endif
