#include "game.h"
#include "core/game_types.h"
#include "core/dirty_flags.h"
#include "core/render_snapshot.h"
#include "core/state_lock.h"
#include "game/game_loop.h"
#include "game/game_worldgen.h"
#include "sim/collapse.h"
#include "sim/civ_colors.h"
#include "sim/diplomacy.h"
#include "sim/disorder.h"
#include "sim/expansion.h"
#include "sim/maritime.h"
#include "sim/plague.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/route_potential.h"
#include "sim/simulation.h"
#include "sim/territory_integrity.h"
#include "sim/simulation_month.h"
#include "sim/simulation_worker.h"
#include "sim/technology.h"
#include "sim/vassal.h"
#include "sim/war.h"
#include "ui/ui.h"
#include "world/ports.h"
#include "world/terrain_query.h"
#include <stdio.h>
#include <string.h>
void game_toggle_auto_run(void) {
    if (!world_generated) return;
    auto_run = !auto_run;
    game_loop_reset();
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
    route_potential_rebuild();
    territory_integrity_repair_capitals();
    diplomacy_update_contacts();
    civilization_colors_debug_check();
    dirty_mark_world();
    world_visual_revision++;
    render_snapshot_publish_from_live_state();
}
int game_tick_auto_run(void) {
    if (!world_generated || !auto_run) return 0;
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
    game_clear_world_tiles();
    config.seed = 1234567u;
    config.random_seed = 0;
    generate_world_with_config(&config);
    world_generated = 1;
    ports_reset_regions();
    regions_generate(region_size_slider);
    ports_ensure_island_ports();
    world_invalidate_region_cache();
    simulation_seed_default_civilizations();
    world_recalculate_territory();
    ports_ensure_island_ports();
    ports_refresh_city_regions();
    route_potential_rebuild();
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
int run_tech10_probe(void) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    FILE *file;
    DWORD start_ms;
    int month_index;
    int max_stage = 0;
    int max_civ = -1;
    int reached_month = -1;
    CreateDirectoryA("logs", NULL);
    file = fopen("logs/tech10_probe.txt", "w");
    if (!file) return 1;
    game_start_blank_world();
    pending_map_size = MAP_SIZE_SMALL;
    initial_civ_count = 8;
    set_active_map_size(pending_map_size);
    diplomacy_reset();
    war_reset();
    simulation_reset_state();
    game_clear_world_tiles();
    config.seed = 987654u;
    config.random_seed = 0;
    generate_world_with_config(&config);
    world_generated = 1;
    ports_reset_regions();
    regions_generate(region_size_slider);
    ports_ensure_island_ports();
    world_invalidate_region_cache();
    simulation_seed_default_civilizations();
    world_recalculate_territory();
    ports_ensure_island_ports();
    ports_refresh_city_regions();
    route_potential_rebuild();
    maritime_rebuild_routes();
    diplomacy_update_contacts();
    start_ms = GetTickCount();
    for (month_index = 0; month_index < 1600 * 12; month_index++) {
        int i;
        simulation_month_run_blocking();
        for (i = 0; i < civ_count; i++) {
            if (!civs[i].alive) continue;
            if (civs[i].tech_stage > max_stage) {
                max_stage = civs[i].tech_stage;
                max_civ = i;
                fprintf(file, "tech_stage=%d year=%d month=%d civ=%d name=%s\n",
                        max_stage, year, month, i, civilization_display_name_for_language(i, 0));
            }
            if (civs[i].tech_stage >= 10 && reached_month < 0) {
                reached_month = month_index + 1;
                max_civ = i;
            }
        }
        if (reached_month >= 0) break;
    }
    fprintf(file, "result reached_stage10=%s elapsed_months=%d elapsed_years=%d max_stage=%d civ=%d sim_ms=%lu avg_ms_per_month=%.3f\n",
            reached_month >= 0 ? "yes" : "no", reached_month >= 0 ? reached_month : month_index,
            (reached_month >= 0 ? reached_month : month_index) / 12,
            max_stage, max_civ, (unsigned long)(GetTickCount() - start_ms),
            month_index > 0 ? (double)(GetTickCount() - start_ms) / (double)month_index : 0.0);
    if (max_civ >= 0) {
        fprintf(file, "stage10_bonus expansion=%d resource=%d tech=%d deep_stability=%d deep_plague=%d defense=%d battle=%d annex=%d\n",
                technology_expansion_percent(max_civ), technology_resource_percent(max_civ),
                technology_progress_percent(max_civ), technology_deep_sea_stability(max_civ),
                technology_deep_sea_plague_percent(max_civ), technology_defense_army_percent(max_civ),
                technology_battle_chance_bonus(max_civ), civs[max_civ].tech_stage >= 10);
    }
    fclose(file);
    return reached_month >= 0 ? 0 : 2;
}
int run_game(void) {
    HINSTANCE instance = GetModuleHandle(NULL);
    const char *class_name = "WorldSimGameWindow";
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;
    render_snapshot_init();
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
    hwnd = CreateWindowExA(0, class_name, "World Sim Game", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
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
    render_snapshot_shutdown();
    return 0;
}
