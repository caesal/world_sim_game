#ifndef WORLD_SIM_RENDER_PANEL_INTERNAL_H
#define WORLD_SIM_RENDER_PANEL_INTERNAL_H

#include "render_common.h"
#include "core/version.h"
#include "sim/diplomacy.h"
#include "sim/simulation.h"
#include "sim/war.h"
#include "ui/ui_layout.h"
#include "ui/ui_types.h"

void draw_top_bar(HDC hdc, RECT client);
int selected_tile_owner(void);
const char *capital_name_for_civ(int civ_id);
void draw_panel_tabs(HDC hdc, RECT client);
void draw_mode_buttons(HDC hdc, RECT client);
void draw_setup_slider(HDC hdc, RECT client, int index, const char *name, int value);
void draw_info_tab(HDC hdc, RECT client, int x, int y, HFONT title_font, HFONT body_font);
void draw_civ_tab(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font);
void draw_diplomacy_tab(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font);
void draw_selection_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font);
void draw_country_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font);
void draw_population_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font);
void draw_plague_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font);
void draw_worldgen_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font);
void draw_debug_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font);
int draw_population_pyramid(HDC hdc, RECT client, int x, int y, int width, int civ_id, HFONT body_font);
void draw_side_panel(HDC hdc, RECT client);
void draw_bottom_bar(HDC hdc, RECT client);
void draw_map_legend(HDC hdc, RECT client);

#endif
