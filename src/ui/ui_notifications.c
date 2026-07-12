#include "ui/ui_notifications.h"

#include "core/game_notifications.h"
#include "core/constants.h"
#include "ui/ui_layout.h"
#include "ui/world_announcement_queue.h"

#include <stdio.h>

void ui_notifications_push(const char *text_en, const char *text_zh) {
    game_notifications_push(text_en, text_zh);
}

int ui_notifications_tick(void) {
    return game_notifications_tick();
}

int ui_notifications_active(void) {
    return game_notifications_active();
}

int ui_notifications_count(void) {
    return game_notifications_count();
}

int ui_notifications_get(int index, UiNotification *out) {
    GameNotification notification;
    if (!out || index < 0 || index >= game_notifications_count()) return 0;
    if (!game_notifications_get(index, &notification)) return 0;
    snprintf(out->text_en, sizeof(out->text_en), "%s", notification.text_en);
    snprintf(out->text_zh, sizeof(out->text_zh), "%s", notification.text_zh);
    out->created_ms = notification.created_ms;
    return 1;
}

RECT ui_notifications_rect_for_announcement(RECT client, int announcement_active) {
    int width = 560;
    RECT viewport = get_map_viewport_rect(client);
    RECT rect;
    if (width > viewport.right - viewport.left - 40) width = viewport.right - viewport.left - 40;
    rect.left = viewport.left + (viewport.right - viewport.left - width) / 2;
    rect.right = rect.left + width;
    rect.top = announcement_active ? get_world_announcement_rect(client).bottom + 8 : TOP_BAR_H + 8;
    rect.bottom = rect.top + UI_NOTIFICATION_MAX * 40;
    return rect;
}

RECT ui_notifications_rect(RECT client) {
    return ui_notifications_rect_for_announcement(client, world_announcement_queue_active());
}

void ui_notifications_invalidate(HWND hwnd) {
    RECT client;
    RECT rect;
    if (!hwnd) return;
    GetClientRect(hwnd, &client);
    rect = ui_notifications_rect(client);
    InvalidateRect(hwnd, &rect, FALSE);
}
