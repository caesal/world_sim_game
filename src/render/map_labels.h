#ifndef WORLD_SIM_MAP_LABELS_H
#define WORLD_SIM_MAP_LABELS_H

#include "render.h"
#include "ui/ui_layout.h"

void draw_map_labels(HDC hdc, RECT client, MapLayout layout);
int map_label_cache_rebuild_count(void);
int map_label_cache_last_rebuild_ms(void);
int map_label_cache_candidate_count(void);
int map_label_cache_drawn_count(void);
const char *map_label_cache_last_reason(void);
const char *map_label_cache_reason_summary(void);

#endif
