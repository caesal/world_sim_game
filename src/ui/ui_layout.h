#ifndef WORLD_SIM_UI_LAYOUT_H
#define WORLD_SIM_UI_LAYOUT_H

#include "ui_types.h"

MapLayout get_map_layout(RECT client);
RECT get_play_button_rect(RECT client);
RECT get_speed_button_rect(RECT client, int index);
RECT get_mode_button_rect(RECT client, int index);
RECT get_map_size_button_rect(RECT client, int index);
RECT get_panel_tab_rect(RECT client, int index);
RECT get_language_button_rect(RECT client);
RECT get_map_legend_box_rect(RECT client);
RECT get_map_legend_toggle_rect(RECT client);
const char *speed_seconds_text(int index);

#endif
