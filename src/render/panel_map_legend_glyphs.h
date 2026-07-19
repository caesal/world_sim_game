#ifndef WORLD_SIM_PANEL_MAP_LEGEND_GLYPHS_H
#define WORLD_SIM_PANEL_MAP_LEGEND_GLYPHS_H

#include "render/icons.h"

#include <windows.h>

void panel_map_legend_draw_glyph_item(HDC hdc, int x, int y, IconId icon,
                                      int harbor, int capital,
                                      const char *name);
void panel_map_legend_draw_capital_pair_item(HDC hdc, int x, int y);

#endif
