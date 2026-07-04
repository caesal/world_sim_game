#ifndef WORLD_SIM_DIPLOMACY_MAP_ANIM_H
#define WORLD_SIM_DIPLOMACY_MAP_ANIM_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core/render_snapshot.h"
#include "ui/ui_types.h"

void diplomacy_map_anim_consume_events(const RenderSnapshot *snapshot);
void diplomacy_map_anim_delay_for_snapshot(const RenderSnapshot *snapshot);
int diplomacy_map_anim_active(void);
int diplomacy_map_anim_active_count(void);
int diplomacy_map_anim_pending_events(const RenderSnapshot *snapshot);
int diplomacy_map_anim_requires_dynamic_paint(const RenderSnapshot *snapshot);
void draw_diplomacy_map_animations(HDC hdc, RECT client, MapLayout layout, const RenderSnapshot *snapshot);
const char *diplomacy_map_anim_source(void);
int diplomacy_map_anim_delayed_waiting_for_snapshot(void);
int diplomacy_map_anim_last_consumed_total(void);
unsigned int diplomacy_map_anim_last_snapshot_revision(void);
int diplomacy_map_anim_last_events_revision(void);
int diplomacy_map_anim_stale_prevented_count(void);
int diplomacy_map_anim_enqueued_count(void);
int diplomacy_map_anim_drawn_count(void);
int diplomacy_map_anim_expired_before_draw_count(void);
int diplomacy_map_anim_overwritten_count(void);
int diplomacy_map_anim_endpoint_reject_count(void);
int diplomacy_map_anim_contact_reject_count(void);
int diplomacy_map_anim_gate_bypass_count(void);
void diplomacy_map_anim_note_cached_paint_blocked(void);
int diplomacy_map_anim_cached_paint_blocked_count(void);
void diplomacy_map_anim_debug_reset(void);

#endif
