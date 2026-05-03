#include "game.h"

#include "game_types.h"
#include "simulation.h"
#include "ui.h"

#include <string.h>

int run_game(void) {
    HINSTANCE instance = GetModuleHandle(NULL);
    const char *class_name = "WorldSimGameWindow";
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;

    reset_simulation();
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassA(&wc)) {
        MessageBoxA(NULL, "Failed to register window class.", "World Sim Game", MB_ICONERROR);
        return 1;
    }

    hwnd = CreateWindowExA(0, class_name, "World Sim Game", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_W, WINDOW_H,
                           NULL, NULL, instance, NULL);
    if (!hwnd) {
        MessageBoxA(NULL, "Failed to create game window.", "World Sim Game", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && (msg.hwnd == hwnd || IsChild(hwnd, msg.hwnd))) {
            if (is_game_shortcut(msg.wParam)) {
                if (handle_shortcut(hwnd, msg.wParam)) continue;
            }
        }
        if (msg.message == WM_CHAR && (msg.hwnd == hwnd || IsChild(hwnd, msg.hwnd))) {
            if (is_game_char_shortcut(msg.wParam)) {
                if (handle_char_shortcut(hwnd, msg.wParam)) continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
