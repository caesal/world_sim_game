#ifndef WORLD_SIM_MAP_LABEL_CACHE_H
#define WORLD_SIM_MAP_LABEL_CACHE_H

#include "core/render_snapshot.h"
#include "ui/ui_layout.h"

void map_label_cache_draw_labels(HDC hdc, RECT client, MapLayout layout,
                                 const RenderSnapshot *snapshot);
int map_label_cache_rebuild_count(void);
int map_label_cache_last_rebuild_ms(void);
int map_label_cache_candidate_count(void);
int map_label_cache_drawn_count(void);
const char *map_label_cache_last_reason(void);
const char *map_label_cache_reason_summary(void);
int map_label_cache_source_rebuild_count(void);
int map_label_cache_placement_rebuild_count(void);
const char *map_label_cache_source_last_reason(void);
const char *map_label_cache_placement_last_reason(void);
int map_label_cache_preview_skip_count(void);
int map_label_cache_measure_hit_count(void);
int map_label_cache_measure_miss_count(void);
void map_label_cache_reset_debug(void);

#endif
