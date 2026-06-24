#include "core/game_notifications.h"

#include <stdio.h>
#include <string.h>

#define GAME_NOTIFICATION_LIFETIME_MS 3200

static GameNotification notifications[GAME_NOTIFICATION_MAX];
static int notification_count;

static int notification_expired(const GameNotification *notification, DWORD now) {
    return notification && (int)(now - notification->created_ms) >= GAME_NOTIFICATION_LIFETIME_MS;
}

void game_notifications_push(const char *text_en, const char *text_zh) {
    int i;
    for (i = GAME_NOTIFICATION_MAX - 1; i > 0; i--) notifications[i] = notifications[i - 1];
    snprintf(notifications[0].text_en, sizeof(notifications[0].text_en), "%s", text_en ? text_en : "");
    snprintf(notifications[0].text_zh, sizeof(notifications[0].text_zh), "%s", text_zh ? text_zh : "");
    notifications[0].created_ms = GetTickCount();
    if (notification_count < GAME_NOTIFICATION_MAX) notification_count++;
}

int game_notifications_tick(void) {
    DWORD now = GetTickCount();
    int old_count = notification_count;
    while (notification_count > 0 && notification_expired(&notifications[notification_count - 1], now)) {
        notification_count--;
    }
    return notification_count != old_count || notification_count > 0;
}

int game_notifications_active(void) {
    return notification_count > 0;
}

int game_notifications_count(void) {
    return notification_count;
}

int game_notifications_get(int index, GameNotification *out) {
    if (!out || index < 0 || index >= notification_count) return 0;
    *out = notifications[index];
    return 1;
}
