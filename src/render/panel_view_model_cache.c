#include "render/panel_view_model_cache.h"

#include "core/game_types.h"
#include "render/panel_country_diplomacy_tooltip.h"
#include "render/panel_view_model_cache_keys.h"
#include "render/profiling_switches.h"
#include "render/render_context.h"
#include "render/render_panel_internal.h"
#include "ui/ui_layout.h"
#include "ui/ui_plague_panel.h"
#include "ui/ui_plague_probability.h"
#include "ui/ui_theme.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    int width;
    int height;
    int valid;
    unsigned int data_key;
    unsigned int ui_key;
    DWORD built_tick;
    int build_ms;
    int refresh_count;
} PanelViewCache;

static PanelViewCache panel_caches[PANEL_CACHE_COUNT];
static int force_refresh = 1;
static int hover_repaint_pending;
static int panel_reason_counts[5];
static int panel_last_reason;
static int panel_full_invalidations = 1;
static int panel_hover_invalidations;
static int panel_last_invalidation_hover;
static int panel_throttle_count;
static PanelViewCacheKind panel_last_kind = PANEL_CACHE_COLLAPSED;
static const char *panel_reason_names[5] = {
    "force", "invalid", "ui", "data", "throttle"
};

static RECT panel_rect_for(RECT client) {
    return get_side_panel_draw_rect(client);
}

static void release_panel_cache(PanelViewCache *cache) {
    if (cache->dc && cache->old_bitmap) {
        SelectObject(cache->dc, cache->old_bitmap);
    }
    if (cache->bitmap) DeleteObject(cache->bitmap);
    if (cache->dc) DeleteDC(cache->dc);
    memset(cache, 0, sizeof(*cache));
}

static int ensure_panel_cache(HDC hdc, PanelViewCache *cache, RECT client) {
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return 0;
    if (cache->dc && cache->width == width && cache->height == height) {
        return 1;
    }
    release_panel_cache(cache);
    cache->dc = CreateCompatibleDC(hdc);
    cache->bitmap = CreateCompatibleBitmap(hdc, width, height);
    if (!cache->dc || !cache->bitmap) {
        release_panel_cache(cache);
        return 0;
    }
    cache->old_bitmap = SelectObject(cache->dc, cache->bitmap);
    cache->width = width;
    cache->height = height;
    return 1;
}

static int should_rebuild(PanelViewCache *cache, PanelViewCacheKind kind,
                          unsigned int ui_key, unsigned int data_key) {
    DWORD now = GetTickCount();
    DWORD interval = (kind == PANEL_CACHE_DEBUG_MAP ||
                      kind == PANEL_CACHE_DEBUG_PERF) ? 1000u : 250u;
    if (force_refresh) {
        panel_last_reason = 0;
        return 1;
    }
    if (!cache->valid) {
        panel_last_reason = 1;
        return 1;
    }
    if (cache->ui_key != ui_key) {
        panel_last_reason = 2;
        return 1;
    }
    if (cache->data_key == data_key) return 0;
    if (now - cache->built_tick >= interval) {
        panel_last_reason = 3;
        return 1;
    }
    panel_last_reason = 4;
    panel_throttle_count++;
    return 0;
}

static void draw_side_panel_cache_body(HDC hdc, RECT client) {
    int old_hover_x = hover_x;
    int old_hover_y = hover_y;
    int old_plague_hover = ui_plague_panel_hover_target();
    hover_x = -1;
    hover_y = -1;
    ui_plague_panel_clear_hover();
    draw_side_panel(hdc, client);
    ui_plague_panel_set_hover_target(old_plague_hover);
    hover_x = old_hover_x;
    hover_y = old_hover_y;
}

static void rebuild_panel_cache(PanelViewCache *cache, RECT client, RECT panel,
                                unsigned int ui_key,
                                unsigned int data_key) {
    DWORD start = GetTickCount();
    fill_rect(cache->dc, panel, ui_theme_color(UI_COLOR_PANEL));
    draw_side_panel_cache_body(cache->dc, client);
    cache->ui_key = ui_key;
    cache->data_key = data_key;
    cache->built_tick = GetTickCount();
    cache->build_ms = (int)(cache->built_tick - start);
    cache->refresh_count++;
    panel_reason_counts[panel_last_reason]++;
    cache->valid = 1;
    force_refresh = 0;
}

static int hover_overlay_active(RECT panel, PanelViewCacheKind kind) {
    int scope = SCORE_TOOLTIP_SCOPE_NONE;
    if (kind != PANEL_CACHE_COUNTRY_DETAIL ||
        panel_tab != PANEL_COUNTRY) return 0;
    if (display_mode == DISPLAY_ALLIANCE && selected_alliance_id >= 0 &&
        alliance_detail_subtab == ALLIANCE_DETAIL_VOTES) {
        scope = SCORE_TOOLTIP_SCOPE_ALLIANCE_VOTES;
    } else if (selected_civ >= 0 &&
               country_detail_subtab == COUNTRY_DETAIL_DIPLOMACY) {
        scope = SCORE_TOOLTIP_SCOPE_COUNTRY_DIPLOMACY;
    } else {
        return 0;
    }
    if (hover_x < panel.left || hover_x >= panel.right ||
        hover_y < panel.top || hover_y >= panel.bottom) return 0;
    return diplomacy_score_tooltip_hover_key_for_scope(
        scope, hover_x, hover_y) > 0;
}

static void draw_hover_overlay(HDC hdc, RECT panel,
                               PanelViewCacheKind kind) {
    if (hover_overlay_active(panel, kind)) {
        diplomacy_score_tooltip_draw(hdc, panel);
    }
}

static int direct_panel_hover_active(PanelViewCacheKind kind) {
    return kind == PANEL_CACHE_PLAGUE &&
           ui_plague_panel_hover_target() != UI_PLAGUE_PANEL_HIT_NONE;
}

void panel_view_model_cache_draw(HDC hdc, RECT client) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    PanelViewCacheKind kind = panel_view_model_cache_kind(snapshot);
    PanelViewCache *cache = &panel_caches[kind];
    RECT panel = panel_rect_for(client);
    unsigned int ui_key;
    unsigned int data_key;
    if (kind == PANEL_CACHE_PLAGUE) {
        ui_plague_probability_sync(snapshot ? &snapshot->plague_state : NULL);
    }
    ui_key = panel_view_model_cache_ui_key(client, kind);
    data_key = panel_view_model_cache_data_key(snapshot, kind);
    panel_last_kind = kind;
    if (kind == PANEL_CACHE_COLLAPSED) {
        draw_side_panel(hdc, client);
        hover_repaint_pending = 0;
        return;
    }
    if (!ensure_panel_cache(hdc, cache, client)) {
        draw_side_panel(hdc, client);
        hover_repaint_pending = 0;
        return;
    }
    if (!profiling_switch_enabled(PROFILING_SWITCH_PANEL_CACHE_REBUILD) &&
        cache->valid) {
        panel_last_reason = 4;
    } else if (should_rebuild(cache, kind, ui_key, data_key)) {
        rebuild_panel_cache(cache, client, panel, ui_key, data_key);
    }
    if ((hover_repaint_pending || direct_panel_hover_active(kind)) &&
        cache->valid &&
        !hover_overlay_active(panel, kind)) {
        draw_side_panel(hdc, client);
        hover_repaint_pending = 0;
        return;
    }
    hover_repaint_pending = 0;
    BitBlt(hdc, panel.left, panel.top, panel.right - panel.left,
           panel.bottom - panel.top, cache->dc, panel.left, panel.top, SRCCOPY);
    draw_hover_overlay(hdc, panel, kind);
    draw_side_panel_handle(hdc, client);
}

void panel_view_model_cache_invalidate(void) {
    force_refresh = 1;
    panel_full_invalidations++;
    panel_last_invalidation_hover = 0;
}

void panel_view_model_cache_invalidate_hover(void) {
    hover_repaint_pending = 1;
    panel_hover_invalidations++;
    panel_last_invalidation_hover = 1;
}

int panel_view_model_cache_last_build_ms(void) {
    return panel_caches[panel_last_kind].build_ms;
}

int panel_view_model_cache_age_ms(void) {
    PanelViewCache *cache = &panel_caches[panel_last_kind];
    if (!cache->valid) return 0;
    return (int)(GetTickCount() - cache->built_tick);
}

int panel_view_model_cache_refresh_count(void) {
    return panel_caches[panel_last_kind].refresh_count;
}

const char *panel_view_model_cache_last_reason(void) {
    return panel_reason_names[panel_last_reason];
}

const char *panel_view_model_cache_key_type(void) {
    return panel_view_model_cache_kind_name(panel_last_kind);
}

const char *panel_view_model_cache_last_invalidation(void) {
    return panel_last_invalidation_hover ? "hover" : "full";
}

int panel_view_model_cache_full_invalidation_count(void) {
    return panel_full_invalidations;
}

int panel_view_model_cache_hover_invalidation_count(void) {
    return panel_hover_invalidations;
}

int panel_view_model_cache_throttle_count(void) {
    return panel_throttle_count;
}

const char *panel_view_model_cache_reason_summary(void) {
    static char text[128];
    snprintf(text, sizeof(text),
             "force %d / ui %d / data %d / hover %d / throttle %d",
             panel_reason_counts[0], panel_reason_counts[2],
             panel_reason_counts[3], panel_hover_invalidations,
             panel_throttle_count);
    return text;
}
