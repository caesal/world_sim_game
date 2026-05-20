#include "ui/ui_invalidation.h"

#include "core/constants.h"
#include "game/game_loop.h"
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

void ui_invalidate_side_panel(HWND hwnd) {
    RECT client;
    RECT panel;
    GetClientRect(hwnd, &client);
    panel.left = side_panel_collapsed ? client.right - SIDE_PANEL_COLLAPSED_W : client.right - side_panel_w;
    panel.top = TOP_BAR_H;
    panel.right = client.right;
    panel.bottom = client.bottom;
    invalidate_clipped(hwnd, panel);
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
    InvalidateRect(hwnd, NULL, FALSE);
}

void ui_invalidate_game_redraw(HWND hwnd, int redraw_flags) {
    if (redraw_flags & GAME_REDRAW_FULL) {
        ui_invalidate_full(hwnd);
        return;
    }
    if (redraw_flags & (GAME_REDRAW_MAP_STATIC | GAME_REDRAW_MAP_DYNAMIC)) ui_invalidate_map_viewport(hwnd);
    if (redraw_flags & GAME_REDRAW_TOP_BAR) ui_invalidate_top_bar(hwnd);
    if (redraw_flags & GAME_REDRAW_BOTTOM_BAR) ui_invalidate_bottom_bar(hwnd);
    if (redraw_flags & GAME_REDRAW_SIDE_PANEL) ui_invalidate_side_panel(hwnd);
}
