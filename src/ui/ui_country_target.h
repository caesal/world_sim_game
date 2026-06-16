#ifndef WORLD_SIM_UI_COUNTRY_TARGET_H
#define WORLD_SIM_UI_COUNTRY_TARGET_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "render/panel_country_actions.h"

typedef enum {
    UI_COUNTRY_TARGET_NONE = 0,
    UI_COUNTRY_TARGET_DECLARE_WAR,
    UI_COUNTRY_TARGET_ALLIANCE,
    UI_COUNTRY_TARGET_VASSALIZE
} UiCountryTargetMode;

typedef struct {
    int active;
    UiCountryTargetMode mode;
    int source_civ;
    int mouse_x;
    int mouse_y;
} UiCountryTargetView;

int ui_country_target_active(void);
UiCountryTargetView ui_country_target_view(void);
int ui_country_target_handle_action_button(HWND hwnd, int source_civ,
                                           CountryVassalActionType action,
                                           int mouse_x, int mouse_y);
int ui_country_target_update_mouse(HWND hwnd, int mouse_x, int mouse_y);
int ui_country_target_handle_left_click(HWND hwnd, int mouse_x, int mouse_y);
int ui_country_target_cancel(HWND hwnd);

#endif
