#ifndef WORLD_SIM_UI_COUNTRY_TARGET_H
#define WORLD_SIM_UI_COUNTRY_TARGET_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "render/panel_country_actions.h"

typedef enum {
    UI_COUNTRY_TARGET_NONE = 0,
    UI_COUNTRY_TARGET_DECLARE_WAR,
    UI_COUNTRY_TARGET_ALLIANCE,
    UI_COUNTRY_TARGET_VASSALIZE,
    UI_COUNTRY_TARGET_ALLIANCE_INVITE,
    UI_COUNTRY_TARGET_ALLIANCE_REMOVE
} UiCountryTargetMode;

typedef struct {
    int active;
    UiCountryTargetMode mode;
    int source_civ;
    int alliance_id;
    int mouse_x;
    int mouse_y;
} UiCountryTargetView;

int ui_country_target_active(void);
UiCountryTargetView ui_country_target_view(void);
int ui_country_target_handle_action_button(HWND hwnd, int source_civ,
                                           CountryVassalActionType action,
                                           int mouse_x, int mouse_y);
int ui_country_target_handle_alliance_button(HWND hwnd, int alliance_id, int source_civ,
                                             UiCountryTargetMode mode, int mouse_x, int mouse_y);
int ui_country_target_is_mode_active(UiCountryTargetMode mode, int alliance_id);
int ui_country_target_update_mouse(HWND hwnd, int mouse_x, int mouse_y);
int ui_country_target_handle_left_click(HWND hwnd, int mouse_x, int mouse_y);
int ui_country_target_cancel(HWND hwnd);

#endif
