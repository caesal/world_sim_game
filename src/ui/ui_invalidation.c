#include "ui/ui_invalidation.h"

#include "core/constants.h"
#include "game/game_loop.h"
#include "render/render.h"
#include "render/panel_view_model_cache.h"
#include "ui/ui_layout.h"
#include "ui/ui_types.h"

static void invalidate_clipped(HWND hwnd, RECT rect) {
    RECT client;
    GetClientRect(hwnd, &client);
    rect.left = max(rect.left, client.left);
    rect.top = max(rect.top, client.top);
    rect.right = min(rect.right, client.right);
    rect.bottom = min(rect.bottom, client.bottom);
    if (rect.right > rect.left && rect.bottom > rect.top) InvalidateRect(hwnd, &rect, FALSE);
}

static void invalidate_side_panel_rect(HWND hwnd) {
    RECT client;
    GetClientRect(hwnd, &client);
    if (!side_panel_collapsed) invalidate_clipped(hwnd, get_side_panel_body_rect(client));
    invalidate_clipped(hwnd, get_side_panel_handle_rect(client));
}

void ui_invalidate_side_panel(HWND hwnd) {
    panel_view_model_cache_invalidate();
    invalidate_side_panel_rect(hwnd);
}

void ui_invalidate_side_panel_immediate(HWND hwnd) {
    ui_invalidate_side_panel(hwnd);
    render_paint_side_panel_now(hwnd);
}

void ui_invalidate_side_panel_hover(HWND hwnd) {
    panel_view_model_cache_invalidate_hover();
    invalidate_side_panel_rect(hwnd);
}

void ui_invalidate_side_panel_handle(HWND hwnd) {
    RECT client;
    RECT rect;
    panel_view_model_cache_invalidate_hover();
    GetClientRect(hwnd, &client);
    rect = get_side_panel_handle_dirty_rect(client);
    invalidate_clipped(hwnd, rect);
}

void ui_invalidate_map_viewport(HWND hwnd) {
    RECT client;
    RECT viewport;
    GetClientRect(hwnd, &client);
    viewport = get_map_viewport_rect(client);
    InflateRect(&viewport, 12, 12);
    invalidate_clipped(hwnd, viewport);
}

void ui_invalidate_top_bar(HWND hwnd) {
    RECT client;
    GetClientRect(hwnd, &client);
    invalidate_clipped(hwnd, (RECT){client.left, client.top, client.right, TOP_BAR_H});
}

void ui_invalidate_bottom_bar(HWND hwnd) {
    RECT client;
    GetClientRect(hwnd, &client);
    invalidate_clipped(hwnd, (RECT){client.left, client.bottom - BOTTOM_BAR_H, client.right, client.bottom});
}

void ui_invalidate_full(HWND hwnd) {
    panel_view_model_cache_invalidate();
    InvalidateRect(hwnd, NULL, FALSE);
}

void ui_invalidate_game_redraw(HWND hwnd, int redraw_flags) {
    if (redraw_flags & GAME_REDRAW_FULL) {
        ui_invalidate_full(hwnd);
        return;
    }
    if (redraw_flags & (GAME_REDRAW_MAP_STATIC | GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_PLAGUE_OVERLAY)) {
        ui_invalidate_map_viewport(hwnd);
    }
    if (redraw_flags & GAME_REDRAW_TOP_BAR) ui_invalidate_top_bar(hwnd);
    if (redraw_flags & GAME_REDRAW_BOTTOM_BAR) ui_invalidate_bottom_bar(hwnd);
    if (redraw_flags & GAME_REDRAW_SIDE_PANEL) ui_invalidate_side_panel(hwnd);
}

void ui_request_panel_state_changed(HWND hwnd) {
    ui_invalidate_side_panel(hwnd);
}

void ui_request_map_static_state_changed(HWND hwnd) {
    ui_invalidate_game_redraw(hwnd, GAME_REDRAW_MAP_STATIC | GAME_REDRAW_SIDE_PANEL);
}
