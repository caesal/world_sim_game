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
    char message[1024];

    if (ui_language == UI_LANG_ZH) {
        snprintf(message, sizeof(message),
                 "World Sim Game Ver %s\n\n本版本新增：\n"
                 "极大地图的城市槽上限现在跟随自然区域上限，避免 1536 个自然区域被 1024 个城市槽卡住。\n"
                 "地图存档版本提升到 13，用于记录更大的城市和瘟疫城市状态容量。\n"
                 "旧存档仍可读取，新存档会使用新的容量版本。\n"
                 "这次修复城市容量瓶颈，但剩余自然区仍可能来自可达性、港口或扩张规则。\n",
                 WORLD_SIM_VERSION);
    } else {
        snprintf(message, sizeof(message),
                 "World Sim Game Ver %s\n\nNew in this version:\n"
                 "Extreme map city capacity now follows the natural-region cap, avoiding the old 1024-city ceiling against 1536 regions.\n"
                 "Map save version is now 13 so larger city and plague-city state capacity is recorded explicitly.\n"
                 "Old saves still load; new saves use the larger capacity version.\n"
                 "This removes the city-cap bottleneck, while unreachable or port-gated natural pockets remain separate expansion behavior.",
                 WORLD_SIM_VERSION);
    }
    show_utf8_message(hwnd, message, pause_menu_button_label(PAUSE_MENU_VERSION_LOG));
}
