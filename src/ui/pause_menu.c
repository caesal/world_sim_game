#include "pause_menu.h"

#include "core/game_types.h"
#include "core/version.h"
#include "ui/ui_types.h"

#include <stdio.h>

RECT pause_menu_panel_rect(RECT client) {
    int width = 380;
    int height = 286;
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
                 "海路航线使用渲染侧平滑曲线，减少折线感。\n"
                 "扩张占领后立即刷新政治填色、边界和标签。\n"
                 "国家详情显示科技与混乱共同作用后的总倍率。\n"
                 "继续保留性能日志、ESC 菜单、保存/读取地图和文明颜色调色板。",
                 WORLD_SIM_VERSION);
    } else {
        snprintf(message, sizeof(message),
                 "World Sim Game Ver %s\n\nNew in this version:\n"
                 "Maritime routes use render-side smoothing to reduce jagged bends.\n"
                 "Political fills, borders, and labels refresh immediately after region claims.\n"
                 "Country detail shows combined technology and disorder modifiers.\n"
                 "Performance logs, ESC menu, map save/load, and civilization color palette remain available.",
                 WORLD_SIM_VERSION);
    }
    show_utf8_message(hwnd, message, pause_menu_button_label(PAUSE_MENU_VERSION_LOG));
}
