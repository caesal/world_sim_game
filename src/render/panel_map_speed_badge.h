#ifndef WORLD_SIM_PANEL_MAP_SPEED_BADGE_H
#define WORLD_SIM_PANEL_MAP_SPEED_BADGE_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stddef.h>

typedef struct {
    RECT render_rect;
    RECT queue_rect;
    RECT status_rect;
    char render_label[16];
    char render_value[16];
    char queue_label[16];
    char queue_value[16];
    char status_text[16];
    COLORREF render_value_color;
    COLORREF status_color;
} PanelMapBottomStatus;

void panel_map_bottom_status_build(PanelMapBottomStatus *out, RECT client, int panel_w,
                                   int language, int render_ms, int pending,
                                   int auto_running, int overloaded);
void panel_map_draw_bottom_status_chips(HDC hdc, RECT client, int panel_w,
                                        int language, int render_ms, int pending,
                                        int auto_running, int overloaded);
void panel_map_format_actual_speed_badge(char *out, size_t size, int actual_ms);
RECT panel_map_actual_speed_badge_rect(RECT client);
int panel_map_actual_speed_badge_inside_frame(RECT client);
void panel_map_draw_actual_speed_badge(HDC hdc, RECT client, int actual_ms);

#endif
