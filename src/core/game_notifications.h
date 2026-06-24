#ifndef WORLD_SIM_GAME_NOTIFICATIONS_H
#define WORLD_SIM_GAME_NOTIFICATIONS_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define GAME_NOTIFICATION_MAX 6
#define GAME_NOTIFICATION_TEXT 160

typedef struct {
    char text_en[GAME_NOTIFICATION_TEXT];
    char text_zh[GAME_NOTIFICATION_TEXT];
    DWORD created_ms;
} GameNotification;

void game_notifications_push(const char *text_en, const char *text_zh);
int game_notifications_tick(void);
int game_notifications_active(void);
int game_notifications_count(void);
int game_notifications_get(int index, GameNotification *out);

#endif
