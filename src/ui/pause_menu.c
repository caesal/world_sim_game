#include "pause_menu.h"

#include "core/game_types.h"
#include "core/version.h"
#include "ui/ui_types.h"

#include <stdio.h>

RECT pause_menu_panel_rect(RECT client) {
    int width = 380;
    int height = 332;
    int cx = (client.left + client.right) / 2;
    int cy = (client.top + client.bottom) / 2;
    RECT rect = {cx - width / 2, cy - height / 2, cx + width / 2, cy + height / 2};
    return rect;
}

RECT pause_menu_button_rect(RECT client, int index) {
    RECT panel = pause_menu_panel_rect(client);
    int left = panel.left + 34;
    int top = panel.top + 78 + index * 46;
    RECT rect = {left, top, panel.right - 34, top + 36};
    return rect;
}

int pause_menu_hit_test(RECT client, int x, int y) {
    int i;

    for (i = 0; i < PAUSE_MENU_BUTTON_COUNT; i++) {
        if (point_in_rect(pause_menu_button_rect(client, i), x, y)) return i;
    }
    return PAUSE_MENU_HIT_NONE;
}

const char *pause_menu_button_label(int index) {
    switch (index) {
        case PAUSE_MENU_RESUME_GAME: return ui_language == UI_LANG_ZH ? "返回游戏" : "Resume Game";
        case PAUSE_MENU_VERSION_LOG: return ui_language == UI_LANG_ZH ? "版本日志" : "Version Log";
        case PAUSE_MENU_SAVE_MAP: return ui_language == UI_LANG_ZH ? "保存地图" : "Save Map";
        case PAUSE_MENU_LOAD_MAP: return ui_language == UI_LANG_ZH ? "读取地图" : "Load Map";
        case PAUSE_MENU_EXIT_GAME: return ui_language == UI_LANG_ZH ? "退出游戏" : "Exit Game";
        default: return "";
    }
}

static void show_utf8_message(HWND hwnd, const char *text, const char *title) {
    WCHAR wide_text[2048];
    WCHAR wide_title[128];

    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide_text, (int)(sizeof(wide_text) / sizeof(wide_text[0])));
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wide_title, (int)(sizeof(wide_title) / sizeof(wide_title[0])));
    MessageBoxW(hwnd, wide_text, wide_title, MB_OK | MB_ICONINFORMATION);
}

void pause_menu_show_version_log(HWND hwnd) {
    char message[2048];

    if (ui_language == UI_LANG_ZH) {
        snprintf(message, sizeof(message),
                 "World Sim Game Ver %s\n\n本版本改进：\n"
                 "重构静态地理生成：宽山脉、固定风场、气候输送、河网、湖泊、河口和三角洲在生成后保持固定。\n"
                 "从生成源头消除海岸斜线和网格伪影，并让极大地图在低海洋比例下可靠生成。\n"
                 "河流按 100/150/225/300%% 缩放逐级显示；300%% 显示完整河网，镜头切换不再重栅格化静态地图。\n"
                 "存档版本升至 20；旧版存档会以本地化提示安全拒绝，不进行迁移。\n",
                 WORLD_SIM_VERSION);
    } else {
        snprintf(message, sizeof(message),
                 "World Sim Game Ver %s\n\nImproved in this version:\n"
                 "Rebuilt static geography with broad mountains, fixed wind, climate transport, river networks, lakes, mouths, and deltas.\n"
                 "Eliminated diagonal coast meshes at generation time and made low-ocean Extreme worlds generate reliably.\n"
                 "Rivers now progress through 100/150/225/300%% zoom, with the full network at 300%% and no static rerasterization on camera changes.\n"
                 "Save version is now 20; older saves are rejected with a localized message and are not migrated.",
                 WORLD_SIM_VERSION);
    }
    show_utf8_message(hwnd, message, pause_menu_button_label(PAUSE_MENU_VERSION_LOG));
}
