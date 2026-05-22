#ifndef WORLD_SIM_PLAGUE_VISUAL_H
#define WORLD_SIM_PLAGUE_VISUAL_H

#include "render.h"
#include "ui/ui_layout.h"

int plague_visual_tick(int elapsed_ms);
int plague_visual_active(void);
int plague_visual_route_intensity(int route_id);
int plague_visual_fog_rebuild_interval_ms(void);
int plague_visual_fog_rebuild_count(void);
int plague_visual_last_fog_rebuild_ms(void);
int plague_visual_last_draw_ms(void);
int plague_visual_data_update_ms(void);
int plague_visual_fog_cache_width(void);
int plague_visual_fog_cache_height(void);
int plague_visual_infected_lane_count(void);
const char *plague_visual_mode_text(void);
const char *plague_visual_last_reason(void);
const char *plague_visual_reason_summary(void);
void draw_plague_visual_regions(HDC hdc, RECT client, MapLayout layout);

#endif
