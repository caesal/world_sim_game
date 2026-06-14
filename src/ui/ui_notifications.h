#ifndef WORLD_SIM_UI_NOTIFICATIONS_H
#define WORLD_SIM_UI_NOTIFICATIONS_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define UI_NOTIFICATION_MAX 6
#define UI_NOTIFICATION_TEXT 160

typedef struct {
    char text_en[UI_NOTIFICATION_TEXT];
    char text_zh[UI_NOTIFICATION_TEXT];
    DWORD created_ms;
} UiNotification;

void ui_notifications_push(const char *text_en, const char *text_zh);
int ui_notifications_tick(void);
int ui_notifications_active(void);
int ui_notifications_count(void);
int ui_notifications_get(int index, UiNotification *out);
RECT ui_notifications_rect(RECT client);
void ui_notifications_invalidate(HWND hwnd);

#endif
