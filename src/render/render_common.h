#ifndef WORLD_SIM_RENDER_COMMON_H
#define WORLD_SIM_RENDER_COMMON_H

#include "render.h"
#include "icons.h"
#include "data/game_tables.h"
#include "ui/ui_layout.h"
#include "world/terrain_query.h"

#include <stddef.h>
#include <stdio.h>

void fill_rect(HDC hdc, RECT rect, COLORREF color);
void fill_rect_alpha(HDC hdc, RECT rect, COLORREF color, BYTE alpha);
void draw_text_line(HDC hdc, int x, int y, const char *text, COLORREF color);
void draw_center_text(HDC hdc, RECT rect, const char *text, COLORREF color);
void draw_text_rect(HDC hdc, RECT rect, const char *text, COLORREF color, unsigned int format);
void measure_text_utf8(HDC hdc, const char *text, SIZE *out_size);
const char *tr(const char *en, const char *zh);
const char *geography_name(Geography geography);
const char *climate_name(Climate climate);
const char *ecology_name(Ecology ecology);
const char *resource_name(ResourceFeature resource);
COLORREF geography_color(Geography geography);
COLORREF climate_color(Climate climate);
COLORREF overview_color(int x, int y);
int point_in_rect_local(RECT rect, int x, int y);
void draw_icon_text_line(HDC hdc, int x, int y, IconId icon, const char *text, COLORREF color);
void format_metric_value(int value, char *buffer, size_t buffer_size);
void draw_metric_box(HDC hdc, RECT rect, IconId icon, const char *label, int value,
                     COLORREF accent, const char *tooltip_text, const char **active_tooltip);
const char *metric_label(const char *en, const char *zh);
RECT metric_grid_rect(int x, int y, int box_w, int box_h, int index);
void draw_tooltip(HDC hdc, RECT client, const char *tooltip_text);

#endif
