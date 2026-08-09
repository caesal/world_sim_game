#include "render/render_partial_ui.h"

#include "core/dirty_flags.h"
#include "core/load_progress.h"
#include "game/game_loop.h"
#include "render/panel_map_speed_badge.h"
#include "render/panel_view_model_cache.h"
#include "render/render_common.h"
#include "render/render_layer_cache.h"
#include "render/render_panel_internal.h"
#include "render/render_static_map_cache.h"
#include "render/render_static_scene.h"
#include "render/render_transient_ui.h"
#include "sim/simulation_worker.h"
#include "ui/color_picker.h"
#include "ui/ui_layout.h"
#include "ui/ui_theme.h"
#include "core/worldgen_progress.h"

static HDC top_bar_dc;
static HBITMAP top_bar_bitmap;
static HBITMAP top_bar_default_bitmap;
static int top_bar_width;
static LayerCache side_panel_cache;

static int rects_intersect(RECT a, RECT b) {
    RECT out;
    return IntersectRect(&out, &a, &b);
}

static int rect_contains(RECT outer, RECT inner) {
    return inner.left >= outer.left && inner.top >= outer.top &&
           inner.right <= outer.right && inner.bottom <= outer.bottom &&
           inner.right > inner.left && inner.bottom > inner.top;
}

static RECT speed_badge_dirty_rect(RECT client) {
    RECT rect = panel_map_actual_speed_badge_rect(client);
    InflateRect(&rect, 2, 2);
    return rect;
}

static int paint_is_speed_badge_update(RECT client, RECT paint) {
    RECT bottom = {client.left, client.bottom - BOTTOM_BAR_H,
                   client.right, client.bottom};
    RECT badge = speed_badge_dirty_rect(client);
    return rect_contains(badge, paint) ||
           (rects_intersect(paint, bottom) && rects_intersect(paint, badge) &&
            paint.top >= badge.top && paint.bottom <= client.bottom);
}

static int paint_is_ui_chrome_only(RECT client, RECT paint) {
    RECT top = {client.left, client.top, client.right, TOP_BAR_H};
    RECT bottom = {client.left, client.bottom - BOTTOM_BAR_H,
                   client.right, client.bottom};
    RECT side = get_side_panel_draw_rect(client);
    RECT handle = get_side_panel_handle_rect(client);
    RECT handle_dirty = get_side_panel_handle_dirty_rect(client);
    if (!side_panel_collapsed && handle.left < side.left) side.left = handle.left;
    return rect_contains(top, paint) || rect_contains(bottom, paint) ||
           rect_contains(side, paint) || rect_contains(handle, paint) ||
           rect_contains(handle_dirty, paint) || paint_is_speed_badge_update(client, paint);
}

static int ensure_top_bar_surface(HDC target, int width) {
    HBITMAP bitmap;
    HBITMAP previous;
    if (top_bar_dc && top_bar_bitmap && top_bar_width == width) return 1;
    if (!top_bar_dc) top_bar_dc = CreateCompatibleDC(target);
    if (!top_bar_dc) return 0;
    bitmap = CreateCompatibleBitmap(target, width, TOP_BAR_H);
    if (!bitmap) return 0;
    if (top_bar_bitmap) {
        SelectObject(top_bar_dc, top_bar_default_bitmap);
        DeleteObject(top_bar_bitmap);
    }
    previous = SelectObject(top_bar_dc, bitmap);
    if (!top_bar_default_bitmap) top_bar_default_bitmap = previous;
    top_bar_bitmap = bitmap;
    top_bar_width = width;
    return 1;
}

static void draw_partial_top_bar(HDC target, RECT client, RECT paint) {
    RECT top = {client.left, client.top, client.right, TOP_BAR_H};
    RECT dirty;
    int width = client.right - client.left;
    if (!IntersectRect(&dirty, &top, &paint)) return;
    if (!ensure_top_bar_surface(target, width)) {
        draw_top_bar(target, client);
        return;
    }
    draw_top_bar(top_bar_dc, client);
    BitBlt(target, dirty.left, dirty.top, dirty.right - dirty.left,
           dirty.bottom - dirty.top, top_bar_dc,
           dirty.left - client.left, dirty.top - client.top, SRCCOPY);
}

int render_partial_ui_map_data_dirty(void) {
    return dirty_render_terrain() || dirty_render_political() ||
           dirty_render_coast() || dirty_render_hydrology() ||
           dirty_render_borders() || dirty_render_maritime() ||
           dirty_render_labels() || dirty_render_plague();
}

int render_partial_ui_stale_indicator_needed(void) {
    if (render_snapshot_age_ms() <= 500) return 0;
    if (simulation_worker_pending_months() > 0 ||
        simulation_worker_visual_backlog() > 0 ||
        simulation_worker_presentation_throttled() || simulation_worker_overloaded()) return 1;
    if (render_partial_ui_map_data_dirty()) return 1;
    return render_static_map_cache_needs_work() || !render_static_scene_complete();
}

void render_partial_ui_draw_stale_indicator(HDC target, RECT client) {
    RECT reset = get_reset_view_button_rect(client);
    RECT year = {client.right / 2 - 112, 9, client.right / 2 + 112, 50};
    RECT badge = {reset.left - 136, reset.top + 3, reset.left - 8, reset.bottom - 3};
    if (badge.left < year.right + 12) badge.left = year.right + 12;
    if (badge.right - badge.left < 96) return;
    fill_rect_alpha(target, badge, RGB(42, 48, 54), 210);
    draw_center_text(target, badge, tr("Updating data", "数据更新中"), RGB(218, 226, 232));
}

int render_partial_ui_can_paint(RECT client, RECT paint,
                                const RenderSnapshot *snapshot) {
    RECT viewport = get_map_viewport_rect(client);
    WorldGenProgress progress;
    (void)snapshot;
    worldgen_progress_get(&progress);
    if (color_picker_active() || pause_menu_open || progress.active || load_progress_active()) return 0;
    if (render_transient_ui_can_partial(client, paint)) return 1;
    if (paint_is_ui_chrome_only(client, paint)) return 1;
    return !rects_intersect(paint, viewport);
}

void render_partial_ui_draw(HDC target, RECT client, RECT paint) {
    RECT top = {client.left, client.top, client.right, TOP_BAR_H};
    RECT bottom = {client.left, client.bottom - BOTTOM_BAR_H,
                   client.right, client.bottom};
    RECT panel = get_side_panel_draw_rect(client);
    RECT handle_dirty = get_side_panel_handle_dirty_rect(client);
    RECT badge = speed_badge_dirty_rect(client);
    if (render_transient_ui_draw_partial(target, client, paint)) return;
    if (rects_intersect(paint, top)) {
        draw_partial_top_bar(target, client, paint);
        if (render_partial_ui_stale_indicator_needed())
            render_partial_ui_draw_stale_indicator(target, client);
    }
    if (rects_intersect(paint, bottom)) draw_bottom_bar(target, client);
    if (rects_intersect(paint, badge))
        panel_map_draw_actual_speed_badge(target, client, game_loop_actual_ms_per_month());
    if (!rects_intersect(paint, panel) &&
        !rects_intersect(paint, handle_dirty)) return;
    if (side_panel_collapsed) {
        panel_view_model_cache_draw(target, client);
    } else if (rects_intersect(paint, panel) &&
               render_layer_cache_ensure(target, &side_panel_cache, client,
                                         get_map_layout(client), side_panel_w, display_mode)) {
        fill_rect(side_panel_cache.dc, panel, ui_theme_color(UI_COLOR_PANEL));
        panel_view_model_cache_draw(side_panel_cache.dc, client);
        BitBlt(target, panel.left, panel.top, panel.right - panel.left, panel.bottom - panel.top,
               side_panel_cache.dc, panel.left, panel.top, SRCCOPY);
    } else if (rects_intersect(paint, panel)) {
        panel_view_model_cache_draw(target, client);
        return;
    }
    if (!side_panel_collapsed && rects_intersect(paint, handle_dirty))
        draw_side_panel_handle(target, client);
}
