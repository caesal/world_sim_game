#include "game.h"

#include "core/game_types.h"
#include "core/dirty_flags.h"
#include "core/state_lock.h"
#include "game/game_loop.h"
#include "sim/collapse.h"
#include "sim/civ_colors.h"
#include "sim/diplomacy.h"
#include "sim/disorder.h"
#include "sim/expansion.h"
#include "sim/maritime.h"
#include "sim/plague.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/simulation_month.h"
#include "sim/simulation_worker.h"
#include "sim/technology.h"
#include "sim/war.h"
#include "ui/ui.h"
#include "world/ports.h"
#include "world/terrain_query.h"
#include "world/world_gen.h"

#include <stdio.h>
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

    auto_run = 0;
    game_loop_reset();
    set_active_map_size(pending_map_size);
    diplomacy_reset();
    war_reset();
    simulation_reset_state();
    clear_world_tiles();
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
    civilization_repair_alive_colors();
    civilization_colors_debug_check();
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
    state_write_lock();
    if (selected_x >= 0 && selected_y >= 0 &&
        is_land(world[selected_y][selected_x].geography) &&
        world[selected_y][selected_x].owner == -1) {
        x = selected_x;
        y = selected_y;
    }
    if (!add_civilization_at(name, symbol, military, logistics, governance, cohesion,
                             production, commerce, innovation, x, y)) {
        state_write_unlock();
        return -1;
    }
    selected_civ = before_count;
    state_write_unlock();
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
    state_write_lock();
    simulation_apply_civilization_edit(civ_id, name, symbol, military, logistics, governance,
                                       cohesion, production, commerce, innovation);
    selected_civ = civ_id;
    state_write_unlock();
    return 1;
}

void game_request_set_civilization_color(int civ_id, Color32 color) {
    int seed_region = -1;

    if (civ_id < 0 || civ_id >= civ_count) return;
    state_write_lock();
    if (civs[civ_id].capital_city >= 0 && civs[civ_id].capital_city < city_count) {
        seed_region = regions_region_for_city(civs[civ_id].capital_city);
    }
    civs[civ_id].color = civilization_pick_distinct_color(civ_id, color, -1, seed_region);
    dirty_mark_territory();
    dirty_mark_labels();
    world_visual_revision++;
    state_write_unlock();
}

void game_request_after_load_map(void) {
    selected_x = -1;
    selected_y = -1;
    selected_civ = -1;
    auto_run = 0;
    game_loop_reset();
    diplomacy_reset();
    war_reset();
    plague_reset();
    regions_claim_cache_reset();
    world_invalidate_region_cache();
    ports_refresh_city_regions();
    diplomacy_update_contacts();
    civilization_colors_debug_check();
    dirty_mark_world();
    world_visual_revision++;
}

int game_request_trigger_civil_unrest(int civ_id) {
    int collapsed;
    char event_text[EVENT_LOG_LEN];

    if (!world_generated || civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) {
        snprintf(event_text, sizeof(event_text), "[Collapse] Civil unrest failed: %s",
                 collapse_trigger_block_reason(civ_id));
        event_log_push(event_text);
        return 0;
    }
    state_write_lock();
    disorder_set(civ_id, 100);
    collapsed = collapse_check_immediate(civ_id, COLLAPSE_CAUSE_CIVIL_UNREST);
    if (collapsed) {
        snprintf(event_text, sizeof(event_text), "[Collapse] Civil unrest triggered in %.64s: %s",
                 civilization_display_name_for_language(civ_id, 0), collapse_last_reason(civ_id));
    } else {
        snprintf(event_text, sizeof(event_text), "[Collapse] Civil unrest failed in %.64s: %s",
                 civilization_display_name_for_language(civ_id, 0), collapse_last_reason(civ_id));
    }
    event_log_push(event_text);
    world_invalidate_region_cache();
    ports_refresh_city_regions();
    maritime_mark_routes_dirty();
    diplomacy_mark_contacts_dirty();
    diplomacy_update_contacts();
    dirty_mark_territory();
    dirty_mark_labels();
    world_visual_revision++;
    state_write_unlock();
    return collapsed;
}

int game_tick_auto_run(void) {
    if (!world_generated) return 0;
    return game_loop_tick_frame();
}

static void probe_write_checkpoint(FILE *file, int checkpoint_year) {
    int owned_regions = 0;
    int active_owned_regions = 0;
    int land_regions = 0;
    int frontier = 0;
    int i;

    for (i = 0; i < region_count; i++) {
        if (!natural_regions[i].alive || natural_regions[i].tile_count <= 0) continue;
        land_regions++;
        if (natural_regions[i].owner_civ >= 0) owned_regions++;
        if (natural_regions[i].owner_civ >= 0 && natural_regions[i].owner_civ < civ_count &&
            civs[natural_regions[i].owner_civ].alive) active_owned_regions++;
    }
    fprintf(file, "year=%d active_owned_regions=%d owned_regions=%d land_regions=%d active_ownership=%d%% wars=%d\n",
            checkpoint_year, active_owned_regions, owned_regions, land_regions,
            land_regions > 0 ? active_owned_regions * 100 / land_regions : 0,
            war_total_started_count());
    for (i = 0; i < civ_count; i++) {
        ExpansionAIDiagnostics ai;
        int owned = 0;
        int r;

        if (!civs[i].alive) continue;
        ai = expansion_ai_diagnostics(i, expansion_resource_score_for_civ(i));
        frontier += ai.nearby_unowned_regions;
        for (r = 0; r < region_count; r++) {
            if (natural_regions[r].alive && natural_regions[r].owner_civ == i) owned++;
        }
        fprintf(file, "  civ=%d name=%s regions=%d land_adj=%d land_near=%d shallow=%d route=%d deep=%d port_candidates=%d global=%d%% desire=%d threshold=%d war_desire=%d\n",
                i, civilization_display_name_for_language(i, 0), owned, ai.land_adjacent_unowned_regions,
                ai.land_nearby_unowned_regions, ai.shallow_sea_reachable_regions, ai.maritime_reachable_regions,
                ai.deep_sea_reachable_regions, ai.port_candidate_regions,
                ai.global_unowned_percent, ai.expansion_desire, ai.expansion_threshold,
                diplomacy_last_war_desire(i));
    }
    fprintf(file, "  total_reachable_unowned_sum=%d\n", frontier);
}

int run_expansion_probe(void) {
    static const int checkpoints[8] = {1, 10, 50, 100, 200, 300, 400, 500};
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    FILE *file;
    int month_index;
    int next_checkpoint = 0;

    CreateDirectoryA("logs", NULL);
    file = fopen("logs/expansion_probe.txt", "w");
    if (!file) return 1;

    game_start_blank_world();
    pending_map_size = MAP_SIZE_MEDIUM;
    initial_civ_count = 20;
    set_active_map_size(pending_map_size);
    diplomacy_reset();
    war_reset();
    simulation_reset_state();
    clear_world_tiles();
    config.seed = 1234567u;
    config.random_seed = 0;
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

    fprintf(file, "World Sim expansion probe seed=%u map=%dx%d initial_civs=%d\n",
            config.seed, MAP_W, MAP_H, initial_civ_count);
    {
        int i;
        int all_stage_zero = 1;
        int saved_stage = civ_count > 0 ? civs[0].tech_stage : 0;
        int stage1_expansion;
        int stage2_expansion;
        for (i = 0; i < civ_count; i++) {
            if (civs[i].tech_stage != 0) all_stage_zero = 0;
        }
        if (civ_count > 0) civs[0].tech_stage = 1;
        stage1_expansion = technology_expansion_percent(0);
        if (civ_count > 0) civs[0].tech_stage = 2;
        stage2_expansion = technology_expansion_percent(0);
        if (civ_count > 0) civs[0].tech_stage = saved_stage;
        fprintf(file, "Technology sanity initial_stage0=%s stage0_expansion=%d stage0_resource=%d stage0_deep=%d stage0_defense=%d stage0_battle=%d stage1_expansion=%d stage2_expansion=%d\n",
                all_stage_zero ? "yes" : "no", technology_expansion_percent(0),
                technology_resource_percent(0), technology_deep_sea_stability(0),
                technology_defense_army_percent(0), technology_battle_chance_bonus(0),
                stage1_expansion, stage2_expansion);
    }
    for (month_index = 0; month_index < 500 * 12; month_index++) {
        simulation_month_run_blocking();
        if (next_checkpoint < 8 && year >= checkpoints[next_checkpoint]) {
            probe_write_checkpoint(file, checkpoints[next_checkpoint]);
            next_checkpoint++;
        }
    }
    fclose(file);
    return 0;
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
    simulation_worker_shutdown();
    return 0;
}
