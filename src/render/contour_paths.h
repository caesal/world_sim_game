#ifndef WORLD_SIM_CONTOUR_PATHS_H
#define WORLD_SIM_CONTOUR_PATHS_H

#include "platform/platform_types.h"

#include "ui/ui_types.h"

void contour_paths_draw_coastline(HDC hdc, RECT client, MapLayout layout);
void contour_paths_draw_country_borders(HDC hdc, RECT client, MapLayout layout);
void contour_paths_draw_province_borders(HDC hdc, RECT client, MapLayout layout);
void contour_paths_draw_region_borders(HDC hdc, RECT client, MapLayout layout);
void contour_paths_coast_stats(int *land_water_edges, int *paths, int *points,
                               int *closed_loops, int *degenerate_loops,
                               int *preserved_tiny_loops);

#endif
