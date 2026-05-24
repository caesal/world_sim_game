#include "render_internal.h"

#include "core/dirty_flags.h"
#include "core/load_progress.h"
#include "core/profiler.h"
#include "core/render_snapshot.h"
#include "render/cartography_layers.h"
#include "render/diplomacy_map_anim.h"
#include "render/load_progress_overlay.h"
#include "render/map_highlight.h"
#include "render/pause_menu_render.h"
#include "render/panel_view_model_cache.h"
#include "render/render_context.h"
#include "render/render_static_map_cache.h"
#include "render/worldgen_progress_overlay.h"
#include "core/worldgen_progress.h"
#include "ui/color_picker.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_theme.h"

#include <stdio.h>

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    int width;
    int height;
    int map_x;
    int map_y;
    int draw_w;
    int draw_h;
    int side_w;
    int display;
    int revision;
    unsigned int key;
    int valid;
} LayerCache;

static LayerCache ui_cache;
static LayerCache side_panel_cache;
static LayerCache window_backbuffer;
static int scene_cache_hits;
static int scene_cache_misses;
static int scene_cache_last_build_ms;
static DWORD last_static_continue_invalidate;

static void release_layer_cache(LayerCache *cache) {
    if (cache->dc && cache->old_bitmap) SelectObject(cache->dc, cache->old_bitmap);
    if (cache->bitmap) DeleteObject(cache->bitmap);
    if (cache->dc) DeleteDC(cache->dc);
    memset(cache, 0, sizeof(*cache));
}

static int ensure_layer_cache(HDC hdc, LayerCache *cache, RECT client, MapLayout layout) {
    int width = client.right - client.left;
    int height = client.bottom - client.top;

    if (width <= 0 || height <= 0) return 0;
    if (!cache->dc || cache->width != width || cache->height != height) {
        release_layer_cache(cache);
        cache->dc = CreateCompatibleDC(hdc);
        cache->bitmap = CreateCompatibleBitmap(hdc, width, height);
        if (!cache->dc || !cache->bitmap) {
            release_layer_cache(cache);
            return 0;
        }
        profiler_add_gdi_recreate();
        cache->old_bitmap = SelectObject(cache->dc, cache->bitmap);
        cache->width = width;
        cache->height = height;
    }
    cache->map_x = layout.map_x;
    cache->map_y = layout.map_y;
    cache->draw_w = layout.draw_w;
    cache->draw_h = layout.draw_h;
    cache->side_w = side_panel_w;
    cache->display = display_mode;
    cache->valid = 1;
    return 1;
}

static void draw_legacy_overlay_nonblocking(HDC hdc, RECT client, MapLayout layout) {
    draw_country_highlight(hdc, client, layout);
}

static void draw_non_plague_map_scene(HDC hdc, RECT client, MapLayout layout,
                                      const RenderSnapshot *snapshot) {
    (void)snapshot;
    draw_cached_static_map_nonblocking(hdc, client, layout);
}

static void draw_stale_ui_indicator(HDC hdc, RECT client) {
    RECT reset = get_reset_view_button_rect(client);
    RECT year_box = {client.right / 2 - 112, 9, client.right / 2 + 112, 50};
    RECT badge = {reset.left - 136, reset.top + 3, reset.left - 8, reset.bottom - 3};
    if (badge.left < year_box.right + 12) badge.left = year_box.right + 12;
    if (badge.right - badge.left < 96) return;
    fill_rect_alpha(hdc, badge, RGB(42, 48, 54), 210);
    draw_center_text(hdc, badge, tr("Updating data", "数据更新中"), RGB(218, 226, 232));
}

static void draw_legacy_ui_nonblocking(HDC hdc, RECT client) {
    MapLayout layout = get_map_layout(client);
    if (ensure_layer_cache(hdc, &ui_cache, client, layout)) {
        BitBlt(ui_cache.dc, 0, 0, ui_cache.width, ui_cache.height, hdc, 0, 0, SRCCOPY);
        draw_top_bar(ui_cache.dc, client);
        draw_bottom_bar(ui_cache.dc, client);
        draw_map_frame_overlay(ui_cache.dc, client);
        draw_map_legend(ui_cache.dc, client);
        panel_view_model_cache_draw(ui_cache.dc, client);
        if (pause_menu_open) draw_pause_menu_overlay(ui_cache.dc, client);
        BitBlt(hdc, 0, 0, ui_cache.width, ui_cache.height, ui_cache.dc, 0, 0, SRCCOPY);
    } else {
        draw_top_bar(hdc, client);
        draw_bottom_bar(hdc, client);
        draw_map_frame_overlay(hdc, client);
        draw_map_legend(hdc, client);
        panel_view_model_cache_draw(hdc, client);
        if (pause_menu_open) draw_pause_menu_overlay(hdc, client);
    }
    if (render_snapshot_age_ms() > 500) draw_stale_ui_indicator(hdc, client);
}

static int rects_intersect(RECT a, RECT b) {
    RECT out;
    return IntersectRect(&out, &a, &b);
}

static RECT side_panel_rect(RECT client) {
    RECT panel;
    RECT handle;
    panel.left = side_panel_collapsed ? client.right - SIDE_PANEL_COLLAPSED_W : client.right - side_panel_w;
    panel.top = TOP_BAR_H;
    panel.right = client.right;
    panel.bottom = client.bottom;
    handle = get_side_panel_handle_rect(client);
    if (handle.left < panel.left) panel.left = handle.left;
    return panel;
}

static void draw_partial_ui(HDC hdc, RECT client, RECT paint) {
    RECT top = {client.left, client.top, client.right, TOP_BAR_H};
    RECT bottom = {client.left, client.bottom - BOTTOM_BAR_H, client.right, client.bottom};
    RECT panel = side_panel_rect(client);
    if (rects_intersect(paint, top)) {
        draw_top_bar(hdc, client);
        if (render_snapshot_age_ms() > 500) draw_stale_ui_indicator(hdc, client);
    }
    if (rects_intersect(paint, bottom)) draw_bottom_bar(hdc, client);
    if (rects_intersect(paint, panel)) {
        if (ensure_layer_cache(hdc, &side_panel_cache, client, get_map_layout(client))) {
            fill_rect(side_panel_cache.dc, panel, ui_theme_color(UI_COLOR_PANEL));
            panel_view_model_cache_draw(side_panel_cache.dc, client);
            BitBlt(hdc, panel.left, panel.top, panel.right - panel.left, panel.bottom - panel.top,
                   side_panel_cache.dc, panel.left, panel.top, SRCCOPY);
        } else {
            panel_view_model_cache_draw(hdc, client);
        }
    }
}

static int can_paint_ui_only(RECT client, RECT paint) {
    RECT viewport = get_map_viewport_rect(client);
    WorldGenProgress progress;
    worldgen_progress_get(&progress);
    if (color_picker_active() || pause_menu_open || progress.active || load_progress_active()) return 0;
    return !rects_intersect(paint, viewport);
}

static void render_world(HDC hdc, RECT client) {
    MapLayout layout = get_map_layout(client);
    const RenderSnapshot *snapshot = render_context_snapshot();
    int snapshot_world_ready = snapshot && snapshot->world_generated;
    WorldGenProgress progress;

    worldgen_progress_get(&progress);
    if (load_progress_active()) {
        RECT viewport = get_map_viewport_rect(client);
        fill_rect(hdc, client, RGB(13, 17, 21));
        draw_legacy_ui_nonblocking(hdc, client);
        fill_rect(hdc, viewport, RGB(13, 17, 21));
        draw_load_progress_overlay(hdc, client);
        return;
    }
    if (progress.active) {
        RECT viewport = get_map_viewport_rect(client);
        fill_rect(hdc, client, RGB(13, 17, 21));
        draw_legacy_ui_nonblocking(hdc, client);
        fill_rect(hdc, viewport, RGB(13, 17, 21));
        draw_worldgen_progress_overlay(hdc, client);
        return;
    }
    draw_non_plague_map_scene(hdc, client, layout, snapshot);
    if (snapshot_world_ready) {
        draw_maritime_routes(hdc, client, layout);
        dirty_clear_render_maritime();
        if (!map_interaction_preview) {
            draw_plague_region_overlay(hdc, client, layout);
            dirty_clear_render_plague();
        }
        draw_legacy_overlay_nonblocking(hdc, client, layout);
        diplomacy_map_anim_consume_events();
        draw_diplomacy_map_animations(hdc, client, layout);
        draw_cities(hdc, layout);
        {
            int labels_dirty = dirty_render_labels();
            draw_map_labels(hdc, client, layout);
            if (labels_dirty) profiler_add_render_rebuild(PROFILER_RENDER_LABEL);
            dirty_clear_render_labels();
        }
        draw_selected_tile(hdc, layout);
    } else {
        dirty_clear_render_maritime();
        dirty_clear_render_plague();
        dirty_clear_render_labels();
    }
    draw_legacy_ui_nonblocking(hdc, client);
    draw_worldgen_progress_overlay(hdc, client);
    draw_load_progress_overlay(hdc, client);
}

void paint_window(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT client;
    const RenderSnapshot *snapshot;
    DWORD render_start = GetTickCount();
    int width;
    int height;
    int ui_only;
    int continue_static_work;

    GetClientRect(hwnd, &client);
    width = client.right - client.left;
    height = client.bottom - client.top;
    snapshot = render_snapshot_acquire();
    render_context_begin(snapshot);
    ui_only = can_paint_ui_only(client, ps.rcPaint);
    continue_static_work = render_static_map_cache_needs_work();
    if (ui_only) {
        draw_partial_ui(hdc, client, ps.rcPaint);
    } else if (ensure_layer_cache(hdc, &window_backbuffer, client, get_map_layout(client))) {
        render_world(window_backbuffer.dc, client);
        color_picker_draw(window_backbuffer.dc, client);
        BitBlt(hdc, 0, 0, width, height, window_backbuffer.dc, 0, 0, SRCCOPY);
    } else {
        render_world(hdc, client);
        color_picker_draw(hdc, client);
    }
    if (!ui_only) continue_static_work = render_static_map_cache_needs_work();
    render_context_end();
    render_snapshot_release(snapshot);
    profiler_record_render_ms((int)(GetTickCount() - render_start));
    EndPaint(hwnd, &ps);
    if (continue_static_work) {
        DWORD now = GetTickCount();
        if ((int)(now - last_static_continue_invalidate) >= 33) {
            last_static_continue_invalidate = now;
            ui_invalidate_map_viewport(hwnd);
        }
    }
}

int render_scene_cache_hits(void) {
    return scene_cache_hits;
}

int render_scene_cache_misses(void) {
    return scene_cache_misses;
}

int render_scene_cache_last_build_ms(void) {
    return scene_cache_last_build_ms;
}

const char *render_scene_cache_last_reason(void) { return "retired"; }
const char *render_scene_cache_reason_summary(void) { return "static map cache draws scene"; }
