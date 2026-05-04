#ifndef WORLD_SIM_BORDER_PATHS_H
#define WORLD_SIM_BORDER_PATHS_H

#include "render.h"
#include "ui/ui_layout.h"

void draw_political_region_fills(HDC hdc, RECT client, MapLayout layout);
void draw_coast_halo(HDC hdc, RECT client, MapLayout layout);
void draw_province_border_paths(HDC hdc, RECT client, MapLayout layout);
void draw_country_border_paths(HDC hdc, RECT client, MapLayout layout);
void draw_coastline_paths(HDC hdc, RECT client, MapLayout layout);
void draw_map_grid_overlay(HDC hdc, RECT client, MapLayout layout);

#endif
