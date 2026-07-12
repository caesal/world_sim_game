#ifndef WORLD_SIM_UI_LAYOUT_H
#define WORLD_SIM_UI_LAYOUT_H

#include "ui_types.h"

typedef enum {
    WORLD_ANNOUNCEMENT_CONTROL_PREVIOUS,
    WORLD_ANNOUNCEMENT_CONTROL_NEXT,
    WORLD_ANNOUNCEMENT_CONTROL_LOCATE,
    WORLD_ANNOUNCEMENT_CONTROL_DISMISS
} WorldAnnouncementControl;

MapLayout get_map_layout(RECT client);
RECT get_map_viewport_rect(RECT client);
RECT get_map_frame_rect(RECT client);
RECT get_map_content_rect(RECT client);
RECT get_map_actual_speed_badge_rect(RECT client);
int ui_side_panel_reserved_width(void);
RECT get_side_panel_body_rect(RECT client);
RECT get_side_panel_draw_rect(RECT client);
RECT get_side_panel_handle_rect(RECT client);
RECT get_side_panel_handle_dirty_rect(RECT client);
int side_panel_handle_hit_test(RECT client, int x, int y);
void ui_map_view_reset(void);
void ui_map_view_clamp(RECT client);
void ui_side_panel_apply_state(RECT client);
void ui_toggle_side_panel(RECT client);
RECT get_bottom_control_row_rect(RECT client);
RECT get_play_button_rect(RECT client);
RECT get_speed_button_rect(RECT client, int index);
RECT get_mode_button_rect(RECT client, int index);
RECT get_map_size_button_rect(RECT client, int index);
RECT get_panel_tab_rect(RECT client, int index);
RECT get_language_button_rect(RECT client);
RECT get_reset_view_button_rect(RECT client);
RECT get_world_announcement_rect(RECT client);
RECT get_world_announcement_control_rect(RECT client, WorldAnnouncementControl control);
RECT get_map_legend_box_rect(RECT client);
RECT get_map_legend_toggle_rect(RECT client);
RECT get_map_legend_hit_rect(RECT client);
const char *speed_seconds_text(int index);
const char *speed_button_icon(int index);

#endif
