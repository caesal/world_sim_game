#ifndef WORLD_SIM_CARTOGRAPHY_LAYERS_H
#define WORLD_SIM_CARTOGRAPHY_LAYERS_H

#include "render/render_common.h"
#include "ui/ui_layout.h"

void draw_cartography_political_fills(HDC hdc, RECT client, MapLayout layout);
void draw_cartography_coast_halo(HDC hdc, RECT client, MapLayout layout);
void draw_cartography_region_borders(HDC hdc, RECT client, MapLayout layout);
void draw_cartography_province_borders(HDC hdc, RECT client, MapLayout layout);
void draw_cartography_country_borders(HDC hdc, RECT client, MapLayout layout);
void draw_cartography_coastline(HDC hdc, RECT client, MapLayout layout);

#endif
