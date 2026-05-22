#ifndef WORLD_SIM_PANEL_VIEW_MODEL_CACHE_H
#define WORLD_SIM_PANEL_VIEW_MODEL_CACHE_H

#include <windows.h>

void panel_view_model_cache_draw(HDC hdc, RECT client);
void panel_view_model_cache_invalidate(void);
int panel_view_model_cache_last_build_ms(void);
int panel_view_model_cache_age_ms(void);
int panel_view_model_cache_refresh_count(void);
const char *panel_view_model_cache_last_reason(void);
const char *panel_view_model_cache_reason_summary(void);

#endif
