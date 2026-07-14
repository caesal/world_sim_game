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
                 "全面重做全局命名瘟疫：周期检查、孢子传播、路线、免疫、死亡、混乱和历史。\n"
                 "瘟疫面板新增实时、影响和历史对比，并优化国家色块、图标和翻页。\n"
                 "新增无瘟疫、小型、中型和大型概率控制；活动瘟疫期间的设置会在结束后生效。\n"
                 "存档版本升至 19；旧版存档会以本地化提示安全拒绝，不进行迁移。\n",
                 WORLD_SIM_VERSION);
    } else {
        snprintf(message, sizeof(message),
                 "World Sim Game Ver %s\n\nImproved in this version:\n"
                 "Rebuilt global named plagues with scheduled checks, spore spread, routes, immunity, mortality, disorder, and history.\n"
                 "Added Live, Impact, and History comparison views with country-color blocks, fitted icons, and compact paging.\n"
                 "Added linked No plague, Small, Medium, and Large probability controls; changes made during an episode apply after it ends.\n"
                 "Save version is now 19; older saves are rejected with a localized message and are not migrated.",
                 WORLD_SIM_VERSION);
    }
    show_utf8_message(hwnd, message, pause_menu_button_label(PAUSE_MENU_VERSION_LOG));
}
