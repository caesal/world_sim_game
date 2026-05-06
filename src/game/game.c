#include "game.h"

#include "core/game_types.h"
#include "core/dirty_flags.h"
#include "game/game_loop.h"
#include "sim/diplomacy.h"
#include "sim/maritime.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/war.h"
#include "ui/ui.h"
#include "world/ports.h"
#include "world/terrain_query.h"
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
            world[y][x].region_id = -1;
        }
    }
    river_path_count = 0;
    maritime_route_count = 0;
    regions_reset();
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
    config.seed = 0u;
    config.random_seed = 1;
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
    regions_generate(region_size_slider);
    world_invalidate_region_cache();
    simulation_seed_default_civilizations();
    world_recalculate_territory();
    ports_refresh_city_regions();
    maritime_rebuild_routes();
    diplomacy_update_contacts();
    dirty_mark_world();
    auto_run = 0;
}

void game_request_regenerate_regions(void) {
    if (!world_generated || civ_count > 0) return;
    regions_generate(region_size_slider);
    selected_x = -1;
    selected_y = -1;
    selected_civ = -1;
}

int game_request_add_civilization_from_selection(const char *name, char symbol,
                                                int military, int logistics,
                                                int governance, int cohesion,
                                                int production, int commerce,
                                                int innovation) {
    int x = -1;
    int y = -1;
    int before_count = civ_count;

    if (!world_generated) return -1;
    if (selected_x >= 0 && selected_y >= 0 &&
        is_land(world[selected_y][selected_x].geography) &&
        world[selected_y][selected_x].owner == -1) {
        x = selected_x;
        y = selected_y;
    }
    if (!add_civilization_at(name, symbol, military, logistics, governance, cohesion,
                             production, commerce, innovation, x, y)) return -1;
    selected_civ = before_count;
    return selected_civ;
}

int game_request_edit_selected_civilization(const char *name, char symbol,
                                            int military, int logistics,
                                            int governance, int cohesion,
                                            int production, int commerce,
                                            int innovation) {
    int civ_id = selected_civ;

    if (civ_id < 0 || civ_id >= civ_count) {
        if (selected_x >= 0 && selected_y >= 0) civ_id = world[selected_y][selected_x].owner;
    }
    if (civ_id < 0 || civ_id >= civ_count) return 0;
    simulation_apply_civilization_edit(civ_id, name, symbol, military, logistics, governance,
                                       cohesion, production, commerce, innovation);
    selected_civ = civ_id;
    return 1;
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
