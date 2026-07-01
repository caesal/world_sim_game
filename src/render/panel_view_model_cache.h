#ifndef WORLD_SIM_PANEL_VIEW_MODEL_CACHE_H
#define WORLD_SIM_PANEL_VIEW_MODEL_CACHE_H

#include "core/render_snapshot.h"

#include <windows.h>

void panel_view_model_cache_draw(HDC hdc, RECT client);
void panel_view_model_cache_invalidate(void);
void panel_view_model_cache_invalidate_hover(void);
int panel_view_model_cache_last_build_ms(void);
int panel_view_model_cache_age_ms(void);
int panel_view_model_cache_refresh_count(void);
const char *panel_view_model_cache_last_reason(void);
const char *panel_view_model_cache_reason_summary(void);
const char *panel_view_model_cache_key_type(void);
const char *panel_view_model_cache_last_invalidation(void);
int panel_view_model_cache_full_invalidation_count(void);
int panel_view_model_cache_hover_invalidation_count(void);
int panel_view_model_cache_throttle_count(void);
unsigned int panel_view_model_cache_probe_decision_key(const SnapshotCiv *civ);

#endif
