#ifndef WORLD_SIM_PAUSE_MENU_H
#define WORLD_SIM_PAUSE_MENU_H

#include "platform/platform_types.h"

#define PAUSE_MENU_BUTTON_COUNT 5

typedef enum {
    PAUSE_MENU_HIT_NONE = -1,
    PAUSE_MENU_RESUME_GAME = 0,
    PAUSE_MENU_VERSION_LOG,
    PAUSE_MENU_SAVE_MAP,
    PAUSE_MENU_LOAD_MAP,
    PAUSE_MENU_EXIT_GAME
} PauseMenuHit;

RECT pause_menu_panel_rect(RECT client);
RECT pause_menu_button_rect(RECT client, int index);
int pause_menu_hit_test(RECT client, int x, int y);
const char *pause_menu_button_label(int index);
void pause_menu_show_version_log(HWND hwnd);

#endif
