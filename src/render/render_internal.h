#ifndef RENDER_INTERNAL_H
#define RENDER_INTERNAL_H

#include "render.h"
#include "icons.h"
#include "core/version.h"
#include "data/game_tables.h"
#include "sim/simulation.h"
#include "sim/diplomacy.h"
#include "sim/war.h"
#include "ui/ui_layout.h"
#include "ui/ui_types.h"
#include "world/terrain_query.h"
#include "world/world_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void fill_rect(HDC hdc, RECT rect, COLORREF color);
void fill_rect_alpha(HDC hdc, RECT rect, COLORREF color, BYTE alpha);
void draw_text_line(HDC hdc, int x, int y, const char *text, COLORREF color);
void draw_center_text(HDC hdc, RECT rect, const char *text, COLORREF color);
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
void draw_metric_box(HDC hdc, RECT rect, IconId icon, const char *label, int value, COLORREF accent, const char *tooltip_text, const char **active_tooltip);
const char *metric_label(const char *en, const char *zh);
RECT metric_grid_rect(int x, int y, int box_w, int box_h, int index);
void draw_tooltip(HDC hdc, RECT client, const char *tooltip_text);
int visible_tile_bounds(RECT client, MapLayout layout, int *min_x, int *max_x, int *min_y, int *max_y);
COLORREF tile_display_color(int x, int y);
int tile_left(MapLayout layout, int x);
int tile_right(MapLayout layout, int x);
int tile_top(MapLayout layout, int y);
int tile_bottom(MapLayout layout, int y);

void draw_crisp_map_surface(HDC hdc, MapLayout layout);
void draw_land_texture(HDC hdc, MapLayout layout, int x, int y);
void draw_rivers(HDC hdc, RECT client, MapLayout layout);
void draw_borders(HDC hdc, RECT client, MapLayout layout);
void draw_cities(HDC hdc, MapLayout layout);
void draw_selected_tile(HDC hdc, MapLayout layout);

void draw_top_bar(HDC hdc, RECT client);
int selected_tile_owner(void);
const char *capital_name_for_civ(int civ_id);
void draw_panel_tabs(HDC hdc, RECT client);
void draw_mode_buttons(HDC hdc, RECT client);
void draw_setup_slider(HDC hdc, RECT client, int index, const char *name, int value);
void draw_info_tab(HDC hdc, RECT client, int x, int y, HFONT title_font, HFONT body_font);
void draw_civ_tab(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font);
void draw_diplomacy_tab(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font);
void draw_side_panel(HDC hdc, RECT client);
void draw_bottom_bar(HDC hdc, RECT client);
void draw_map_legend(HDC hdc, RECT client);

#endif
