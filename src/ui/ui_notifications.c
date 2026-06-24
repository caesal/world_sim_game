#include "ui/ui_notifications.h"

#include "core/game_notifications.h"
#include "core/constants.h"

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

RECT ui_notifications_rect(RECT client) {
    int width = 560;
    RECT rect;
    if (width > client.right - client.left - 40) width = client.right - client.left - 40;
    rect.left = client.left + (client.right - client.left - width) / 2;
    rect.right = rect.left + width;
    rect.top = TOP_BAR_H + 8;
    rect.bottom = rect.top + UI_NOTIFICATION_MAX * 40;
    return rect;
}

void ui_notifications_invalidate(HWND hwnd) {
    RECT client;
    RECT rect;
    if (!hwnd) return;
    GetClientRect(hwnd, &client);
    rect = ui_notifications_rect(client);
    InvalidateRect(hwnd, &rect, FALSE);
}
