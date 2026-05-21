#ifndef WORLD_SIM_PANEL_COUNTRY_EVENTS_H
#define WORLD_SIM_PANEL_COUNTRY_EVENTS_H

#define WIN32_LEAN_AND_MEAN
#include "platform/platform_types.h"

#include "ui/ui_widgets.h"

void country_recent_events_reset_hit(void);
void draw_country_recent_events(HDC hdc, UiCursor *cursor, int civ_id);
int country_recent_events_scroll_hit_test(int mouse_x, int mouse_y);
int country_recent_events_scroll(int item_delta);
int country_recent_events_country_hit_test(int mouse_x, int mouse_y);

#endif
