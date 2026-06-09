#ifndef WORLD_SIM_RENDER_STATIC_SCENE_H
#define WORLD_SIM_RENDER_STATIC_SCENE_H

#include "core/render_snapshot.h"
#include "render/render_common.h"

void render_static_scene_draw(HDC hdc, RECT client, MapLayout layout,
                              const RenderSnapshot *snapshot);
void render_static_scene_request_continue(HWND hwnd, int continue_static_work);
int render_static_scene_presented_current(void);
int render_static_scene_presentable(void);
int render_static_scene_complete(void);
int render_static_scene_fully_current(void);
int render_static_scene_no_safe_frames(void);
int render_static_scene_stale_safe_age_ms(void);
int render_scene_cache_hits(void);
int render_scene_cache_misses(void);
int render_scene_cache_last_build_ms(void);
int render_scene_cache_last_reason_code(void);
const char *render_scene_cache_last_reason(void);
const char *render_static_scene_status_summary(void);
void render_static_scene_reset_debug(void);

#endif
