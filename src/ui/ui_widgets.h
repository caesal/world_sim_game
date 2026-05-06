#ifndef WORLD_SIM_UI_WIDGETS_H
#define WORLD_SIM_UI_WIDGETS_H

#include "render/render_common.h"
#include "render/icons.h"
#include "ui/ui_theme.h"

typedef struct {
    int x;
    int y;
    int width;
    int bottom;
} UiCursor;

UiCursor ui_cursor(int x, int y, int width, int bottom);
void ui_section(HDC hdc, UiCursor *cursor, const char *title);
void ui_row_text(HDC hdc, UiCursor *cursor, const char *label, const char *value);
void ui_row_int(HDC hdc, UiCursor *cursor, const char *label, int value);
void ui_stat_chip(HDC hdc, RECT rect, const char *label, int value, COLORREF accent);
void ui_metric_chip(HDC hdc, RECT rect, IconId icon, const char *label, int value, COLORREF accent);
void ui_progress_bar(HDC hdc, RECT rect, int value, int max_value, COLORREF color);
RECT ui_take_rect(UiCursor *cursor, int height);

#endif
