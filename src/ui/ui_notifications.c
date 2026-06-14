#include "ui/ui_notifications.h"

#include "core/constants.h"

#include <stdio.h>
#include <string.h>

#define UI_NOTIFICATION_LIFETIME_MS 3200

static UiNotification notifications[UI_NOTIFICATION_MAX];
static int notification_count;

static int notification_expired(const UiNotification *notification, DWORD now) {
    return notification && (int)(now - notification->created_ms) >= UI_NOTIFICATION_LIFETIME_MS;
}

void ui_notifications_push(const char *text_en, const char *text_zh) {
    int i;
    for (i = UI_NOTIFICATION_MAX - 1; i > 0; i--) notifications[i] = notifications[i - 1];
    snprintf(notifications[0].text_en, sizeof(notifications[0].text_en), "%s", text_en ? text_en : "");
    snprintf(notifications[0].text_zh, sizeof(notifications[0].text_zh), "%s", text_zh ? text_zh : "");
    notifications[0].created_ms = GetTickCount();
    if (notification_count < UI_NOTIFICATION_MAX) notification_count++;
}

int ui_notifications_tick(void) {
    DWORD now = GetTickCount();
    int old_count = notification_count;
    while (notification_count > 0 && notification_expired(&notifications[notification_count - 1], now)) {
        notification_count--;
    }
    return notification_count != old_count || notification_count > 0;
}

int ui_notifications_active(void) {
    return notification_count > 0;
}

int ui_notifications_count(void) {
    return notification_count;
}

int ui_notifications_get(int index, UiNotification *out) {
    if (!out || index < 0 || index >= notification_count) return 0;
    *out = notifications[index];
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
