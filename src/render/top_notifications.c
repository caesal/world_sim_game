#include "render/top_notifications.h"

#include "render/render_common.h"
#include "ui/ui_notifications.h"

static int notification_alpha(DWORD created_ms, DWORD now) {
    int age = (int)(now - created_ms);
    if (age < 2200) return 226;
    return clamp(226 * (3200 - age) / 1000, 0, 226);
}

void draw_top_notifications(HDC hdc, RECT client) {
    RECT stack = ui_notifications_rect(client);
    DWORD now = GetTickCount();
    int i;
    for (i = 0; i < ui_notifications_count(); i++) {
        UiNotification notification;
        int alpha;
        RECT item;
        RECT text;
        HBRUSH border;
        if (!ui_notifications_get(i, &notification)) continue;
        alpha = notification_alpha(notification.created_ms, now);
        if (alpha <= 0) continue;
        item.left = stack.left;
        item.right = stack.right;
        item.top = stack.top + i * 38;
        item.bottom = item.top + 30;
        text = item;
        text.left += 16;
        text.right -= 16;
        fill_rect_alpha(hdc, item, RGB(30, 34, 40), (BYTE)alpha);
        border = CreateSolidBrush(RGB(178, 156, 96));
        if (border) {
            FrameRect(hdc, &item, border);
            DeleteObject(border);
        }
        draw_text_rect(hdc, text, tr(notification.text_en, notification.text_zh),
                       RGB(248, 244, 222), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
}
