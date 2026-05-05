#include "game.h"

#include "core/game_types.h"
#include "core/dirty_flags.h"
#include "game/game_loop.h"
#include "sim/diplomacy.h"
#include "sim/maritime.h"
#include "sim/ports.h"
#include "sim/simulation.h"
#include "sim/war.h"
#include "ui/ui.h"
#include "world/ports.h"
#include "world/world_gen.h"

#include <string.h>

static void clear_world_tiles(void) {
    int x;
    int y;

    for (y = 0; y < MAX_MAP_H; y++) {
        for (x = 0; x < MAX_MAP_W; x++) {
            memset(&world[y][x], 0, sizeof(world[y][x]));
            world[y][x].geography = GEO_OCEAN;
            world[y][x].climate = CLIMATE_OCEANIC;
            world[y][x].owner = -1;
            world[y][x].province_id = -1;
        }
    }
    river_path_count = 0;
    maritime_route_count = 0;
}

static void game_start_blank_world(void) {
    set_active_map_size(MAP_SIZE_MEDIUM);
    simulation_reset_state();
    clear_world_tiles();
    selected_x = -1;
    selected_y = -1;
    selected_civ = -1;
    auto_run = 0;
    world_generated = 0;
    dirty_mark_world();
}

void game_toggle_auto_run(void) {
    if (!world_generated) return;
    auto_run = !auto_run;
    game_loop_reset();
}

static WorldGenConfig game_world_gen_config_from_globals(void) {
    WorldGenConfig config;

    config.ocean = ocean_slider;
    config.continent = continent_slider;
    config.relief = relief_slider;
    config.moisture = moisture_slider;
    config.drought = drought_slider;
    config.vegetation = vegetation_slider;
    config.bias_forest = bias_forest_slider;
    config.bias_desert = bias_desert_slider;
    config.bias_mountain = bias_mountain_slider;
    config.bias_wetland = bias_wetland_slider;
    return config;
}

void game_request_new_world(void) {
    WorldGenConfig config = game_world_gen_config_from_globals();

    set_active_map_size(pending_map_size);
    diplomacy_reset();
    war_reset();
    simulation_reset_state();
    clear_world_tiles();
    game_loop_reset();
    dirty_mark_world();
    selected_x = -1;
    selected_y = -1;
    selected_civ = -1;
    generate_world_with_config(&config);
    world_generated = 1;
    ports_reset_regions();
    world_invalidate_region_cache();
    simulation_seed_default_civilizations();
    world_recalculate_territory();
    ports_refresh_city_regions();
    maritime_rebuild_routes();
    diplomacy_update_contacts();
    dirty_mark_world();
    auto_run = 0;
}

int game_tick_auto_run(void) {
    if (!world_generated) return 0;
    return game_loop_tick_frame();
}

int run_game(void) {
    HINSTANCE instance = GetModuleHandle(NULL);
    const char *class_name = "WorldSimGameWindow";
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;

    game_start_blank_world();
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;

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
