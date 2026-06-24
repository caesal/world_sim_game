#include "game/game.h"

#include "core/game_types.h"
#include "core/worldgen_progress.h"
#include "game/game_worldgen.h"
#include "sim/diplomacy.h"
#include "sim/maritime.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/route_potential.h"
#include "sim/simulation.h"
#include "sim/war.h"
#include "world/ports.h"
#include "world/terrain_query.h"
#include "world/world_gen.h"

#include <stdio.h>

static WorldGenConfig probe_config(void) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    config.seed = 3300337u;
    config.random_seed = 0;
    config.ocean = 47;
    config.continent = 58;
    config.relief = 61;
    config.moisture = 43;
    config.drought = 52;
    config.vegetation = 57;
    config.bias_forest = 62;
    config.bias_desert = 41;
    config.bias_mountain = 66;
    config.bias_wetland = 39;
    return config;
}

static void reset_probe_world(void) {
    pending_map_size = MAP_SIZE_EXTREME;
    initial_civ_count = 26;
    region_size_slider = 34;
    set_active_map_size(pending_map_size);
    diplomacy_reset();
    war_reset();
    simulation_reset_state();
    game_clear_world_tiles();
    selected_x = -1;
    selected_y = -1;
    selected_civ = -1;
    auto_run = 0;
    world_generated = 0;
}

int run_worldgen_probe(void) {
    WorldGenConfig config = probe_config();
    RoutePotentialStats route_stats;
    WorldGenProgress progress;
    unsigned long start_ms;
    unsigned long route_start_ms;
    FILE *file;

    CreateDirectoryA("logs", NULL);
    file = fopen("logs/worldgen_probe.txt", "w");
    if (!file) return 1;
    reset_probe_world();
    worldgen_progress_begin();
    start_ms = GetTickCount();
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
    route_start_ms = GetTickCount();
    route_potential_rebuild();
    route_potential_stats(&route_stats);
    worldgen_progress_record_route_stats(route_stats.deep_bridge_candidates,
                                         route_stats.shallow_edges, route_stats.deep_edges);
    worldgen_progress_record_stage_ms(WORLDGEN_ROUTE_POTENTIAL_SHALLOW, route_stats.shallow_ms);
    worldgen_progress_record_stage_ms(WORLDGEN_ROUTE_POTENTIAL_DEEP, route_stats.deep_ms);
    maritime_rebuild_routes();
    diplomacy_update_contacts();
    worldgen_progress_record_total_ms((int)(GetTickCount() - start_ms));
    worldgen_progress_get(&progress);
    fprintf(file, "probe=worldgen seed=%u map=%dx%d civs=%d regions=%d route_wall_ms=%lu total_ms=%d\n",
            config.seed, MAP_W, MAP_H, civ_count, region_count,
            GetTickCount() - route_start_ms, progress.total_ms);
    fprintf(file, "phase=route shallow_ms=%d deep_ms=%d nodes=%d shallow_edges=%d deep_edges=%d candidates=%d bridges=%d disconnected=%d rejected_path=%d rejected_deep=%d rejected_near=%d truncated=%d\n",
            route_stats.shallow_ms, route_stats.deep_ms, route_stats.node_count,
            route_stats.shallow_edges, route_stats.deep_edges,
            route_stats.deep_bridge_candidates, route_stats.deep_bridge_count,
            route_stats.disconnected_networks, route_stats.rejected_no_path,
            route_stats.rejected_no_deep_water, route_stats.rejected_near_shore,
            route_stats.rejected_truncated);
    fclose(file);
    return 0;
}
