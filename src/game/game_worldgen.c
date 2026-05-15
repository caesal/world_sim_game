#include "game/game_worldgen.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "core/render_snapshot.h"
#include "core/worldgen_progress.h"
#include "data/province_names.h"
#include "game/game_loop.h"
#include "sim/civ_colors.h"
#include "sim/diplomacy.h"
#include "sim/maritime.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/route_potential.h"
#include "sim/simulation.h"
#include "sim/war.h"
#include "ui/ui_types.h"
#include "world/ports.h"
#include "world/terrain_query.h"

#include <string.h>

void game_clear_world_tiles(void) {
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
    route_potential_reset();
}

WorldGenConfig game_world_gen_config_from_globals(void) {
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

static void repaint_generation(HWND hwnd) {
    if (!hwnd) return;
    InvalidateRect(hwnd, NULL, FALSE);
    UpdateWindow(hwnd);
}

static void repaint_generation_callback(void *user_data) {
    repaint_generation((HWND)user_data);
}

static DWORD begin_generation_stage(HWND hwnd, WorldGenStage stage, int current, int total) {
    worldgen_progress_update_stage(stage, current, total);
    repaint_generation(hwnd);
    return GetTickCount();
}

static void end_generation_stage(HWND hwnd, WorldGenStage stage, DWORD start_ms) {
    worldgen_progress_update_stage(stage, 1, 1);
    repaint_generation(hwnd);
    worldgen_progress_record_stage_ms(stage, (int)(GetTickCount() - start_ms));
}

void game_request_new_world_with_progress(HWND hwnd) {
    WorldGenConfig config = game_world_gen_config_from_globals();
    DWORD total_start;
    DWORD stage_start;
    RoutePotentialStats route_stats;

    worldgen_progress_begin();
    worldgen_progress_set_repaint_callback(repaint_generation_callback, hwnd);
    total_start = GetTickCount();
    auto_run = 0;
    game_loop_reset();
    set_active_map_size(pending_map_size);
    diplomacy_reset();
    war_reset();
    simulation_reset_state();
    game_clear_world_tiles();
    world_generated = 0;
    dirty_mark_world();
    selected_x = -1;
    selected_y = -1;
    selected_civ = -1;
    map_zoom_percent = 100;
    map_offset_x = 0;
    map_offset_y = 0;
    map_view_auto_centered = 1;
    display_mode = DISPLAY_POLITICAL;

    stage_start = begin_generation_stage(hwnd, WORLDGEN_TERRAIN, 0, 1);
    generate_world_with_config(&config);
    world_generated = 1;
    end_generation_stage(hwnd, WORLDGEN_TERRAIN, stage_start);

    stage_start = begin_generation_stage(hwnd, WORLDGEN_REGIONS, 0, 1);
    ports_reset_regions();
    regions_generate(region_size_slider);
    end_generation_stage(hwnd, WORLDGEN_REGIONS, stage_start);

    stage_start = begin_generation_stage(hwnd, WORLDGEN_PORTS, 0, 1);
    ports_ensure_island_ports();
    province_names_assign_all();
    world_invalidate_region_cache();
    end_generation_stage(hwnd, WORLDGEN_PORTS, stage_start);

    stage_start = begin_generation_stage(hwnd, WORLDGEN_CIV_PLACEMENT, 0, max(1, initial_civ_count));
    simulation_seed_default_civilizations();
    world_recalculate_territory();
    ports_ensure_island_ports();
    ports_refresh_city_regions();
    end_generation_stage(hwnd, WORLDGEN_CIV_PLACEMENT, stage_start);

    stage_start = begin_generation_stage(hwnd, WORLDGEN_ROUTE_POTENTIAL_SHALLOW, 0, 1);
    route_potential_rebuild();
    route_potential_stats(&route_stats);
    worldgen_progress_record_route_stats(route_stats.deep_bridge_candidates,
                                         route_stats.shallow_edges, route_stats.deep_edges);
    worldgen_progress_record_stage_ms(WORLDGEN_ROUTE_POTENTIAL_SHALLOW, (int)(GetTickCount() - stage_start));

    stage_start = begin_generation_stage(hwnd, WORLDGEN_FINALIZE, 0, 1);
    maritime_rebuild_routes();
    diplomacy_update_contacts();
    civilization_repair_alive_colors();
    civilization_colors_debug_check();
    dirty_mark_world();
    auto_run = 0;
    render_snapshot_publish_from_live_state();
    end_generation_stage(hwnd, WORLDGEN_FINALIZE, stage_start);
    worldgen_progress_record_total_ms((int)(GetTickCount() - total_start));
    worldgen_progress_finish();
    worldgen_progress_set_repaint_callback(NULL, NULL);
    repaint_generation(hwnd);
}

void game_request_new_world(void) {
    game_request_new_world_with_progress(NULL);
}
