#ifndef WORLD_SIM_PANEL_DEBUG_H
#define WORLD_SIM_PANEL_DEBUG_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int debug_panel_event_filter_hit_test(RECT client, int mouse_x, int mouse_y);
int debug_panel_subtab_hit_test(RECT client, int mouse_x, int mouse_y);
int debug_panel_map_mode_hit_test(RECT client, int mouse_x, int mouse_y);
int debug_panel_event_log_hit_test(RECT client, int mouse_x, int mouse_y);
int debug_panel_event_top_hit_test(RECT client, int mouse_x, int mouse_y);
int debug_panel_event_country_hit_test(RECT client, int mouse_x, int mouse_y);
int debug_panel_event_clear_highlight_hit_test(RECT client, int mouse_x, int mouse_y);
int debug_panel_event_scrollbar_hit_test(RECT client, int mouse_x, int mouse_y);
void debug_panel_event_scrollbar_begin_drag(int mouse_y);
int debug_panel_event_scrollbar_drag(int mouse_y);
void debug_panel_event_scrollbar_end_drag(void);
int debug_panel_event_scrollbar_is_dragging(void);
void debug_panel_event_log_scroll(int item_delta);
void debug_panel_event_log_top(void);

#endif
