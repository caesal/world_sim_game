#ifndef WORLD_SIM_MAP_HIGHLIGHT_H
#define WORLD_SIM_MAP_HIGHLIGHT_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ui/ui_types.h"

void draw_country_highlight(HDC hdc, RECT client, MapLayout layout);
int map_highlight_overlay_call_count(void);
int map_highlight_overlay_recreate_count(void);
int map_highlight_overlay_reuse_count(void);
int map_highlight_overlay_last_ms(void);
int map_highlight_overlay_last_tiles(void);
int map_highlight_overlay_peak_ms(void);
int map_highlight_overlay_total_clears(void);
int map_highlight_overlay_last_clears(void);
int map_highlight_overlay_last_requests(void);
int map_highlight_overlay_last_rects(void);
int map_highlight_edge_call_count(void);
int map_highlight_edge_last_requests(void);
int map_highlight_edge_last_tiles(void);
int map_highlight_edge_last_segments(void);
int map_highlight_edge_last_pen_creates(void);
int map_highlight_edge_last_ms(void);
int map_highlight_edge_peak_ms(void);
int map_highlight_edge_cache_hits(void);
int map_highlight_edge_cache_misses(void);
int map_highlight_edge_geometry_last_ms(void);
int map_highlight_edge_geometry_peak_ms(void);
int map_highlight_edge_layer_last_rebuild_ms(void);
int map_highlight_edge_layer_peak_rebuild_ms(void);
int map_highlight_edge_layer_last_blit_ms(void);
int map_highlight_edge_layer_peak_blit_ms(void);
int map_highlight_edge_layer_last_hit_blit_ms(void);
int map_highlight_edge_layer_hit_only_blits(void);
int map_highlight_focus_call_count(void);
int map_highlight_focus_last_requests(void);
int map_highlight_focus_last_rings(void);
int map_highlight_focus_last_pen_creates(void);
int map_highlight_focus_last_fallback_tiles(void);
int map_highlight_focus_last_ms(void);
int map_highlight_focus_peak_ms(void);
int map_highlight_total_paint_count(void);
int map_highlight_total_last_ms(void);
int map_highlight_total_peak_ms(void);
int map_highlight_last_selected_request_present(void);
int map_highlight_last_alliance_request_count(void);
int map_highlight_last_war_request_count(void);
int map_highlight_last_vassal_request_count(void);
void map_highlight_overlay_reset_debug(void);

#endif
