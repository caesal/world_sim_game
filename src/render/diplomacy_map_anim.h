#ifndef WORLD_SIM_DIPLOMACY_MAP_ANIM_H
#define WORLD_SIM_DIPLOMACY_MAP_ANIM_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core/render_snapshot.h"
#include "ui/ui_types.h"

void diplomacy_map_anim_consume_events(const RenderSnapshot *snapshot);
void diplomacy_map_anim_delay_for_snapshot(const RenderSnapshot *snapshot);
int diplomacy_map_anim_active(void);
void draw_diplomacy_map_animations(HDC hdc, RECT client, MapLayout layout);
const char *diplomacy_map_anim_source(void);
int diplomacy_map_anim_delayed_waiting_for_snapshot(void);
int diplomacy_map_anim_last_consumed_total(void);
unsigned int diplomacy_map_anim_last_snapshot_revision(void);
int diplomacy_map_anim_last_events_revision(void);
int diplomacy_map_anim_stale_prevented_count(void);

#endif
