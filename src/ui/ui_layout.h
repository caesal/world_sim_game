#ifndef WORLD_SIM_UI_LAYOUT_H
#define WORLD_SIM_UI_LAYOUT_H

#include "ui_types.h"

MapLayout get_map_layout(RECT client);
RECT get_map_viewport_rect(RECT client);
RECT get_side_panel_handle_rect(RECT client);
int side_panel_handle_hit_test(RECT client, int x, int y);
void ui_map_view_reset(void);
void ui_map_view_clamp(RECT client);
void ui_side_panel_apply_state(RECT client);
void ui_toggle_side_panel(RECT client);
RECT get_play_button_rect(RECT client);
RECT get_speed_button_rect(RECT client, int index);
RECT get_mode_button_rect(RECT client, int index);
RECT get_map_size_button_rect(RECT client, int index);
RECT get_panel_tab_rect(RECT client, int index);
RECT get_language_button_rect(RECT client);
RECT get_reset_view_button_rect(RECT client);
RECT get_map_legend_box_rect(RECT client);
RECT get_map_legend_toggle_rect(RECT client);
const char *speed_seconds_text(int index);
const char *speed_button_icon(int index);

#endif
