#include "game/game.h"

#include "core/game_types.h"
#include "core/worldgen_progress.h"
#include "data/country_names.h"
#include "data/province_names.h"
#include "game/game_worldgen.h"
#include "game/game_worldgen_commit_probe.h"
#include "game/game_worldgen_diagnostics_probe.h"
#include "game/game_worldgen_failure_probe.h"
#include "game/game_worldgen_hydrology_probe.h"
#include "game/game_worldgen_lake_lifecycle_probe.h"
#include "game/game_worldgen_landform_semantics_probe.h"
#include "game/game_worldgen_live_validation_probe.h"
#include "game/game_worldgen_mountain_shape_probe.h"
#include "game/game_worldgen_physical_probe.h"
#include "game/game_worldgen_region_water_probe.h"
#include "game/game_worldgen_river_payload_probe.h"
#include "game/game_worldgen_snapshot_recovery_probe.h"
#include "game/game_worldgen_terrain_probe.h"
#include "sim/diplomacy.h"
#include "sim/maritime.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/route_potential.h"
#include "sim/simulation.h"
#include "sim/war.h"
#include "world/ports.h"
#include "world/river_path_validation.h"
#include "world/terrain_query.h"
#include "world/world_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void count_heritage_queue(int count, int counts[CIV_HERITAGE_COUNT]) {
    int queue[MAX_CIVS];
    memset(counts, 0, sizeof(int) * CIV_HERITAGE_COUNT);
    srand((unsigned int)(7700 + count));
    simulation_seed_build_default_heritage_queue(count, queue);
    for (int i = 0; i < count; i++) {
        if (queue[i] >= 0 && queue[i] < CIV_HERITAGE_COUNT) counts[queue[i]]++;
    }
}

static int distribution_ok(const int counts[CIV_HERITAGE_COUNT], int total) {
    int min_count = total, max_count = 0, sum = 0;
    for (int h = 0; h < CIV_HERITAGE_COUNT; h++) {
        if (counts[h] < min_count) min_count = counts[h];
        if (counts[h] > max_count) max_count = counts[h];
        sum += counts[h];
    }
    return sum == total && max_count - min_count <= 1;
}

static int check_heritage_setup(FILE *file) {
    int ok = CIV_HERITAGE_COUNT == 4;
    int counts[CIV_HERITAGE_COUNT];
    int distribution_all_ok = 1;
    int west = country_name_count_for_heritage(CIV_HERITAGE_WESTERN);
    int east = country_name_count_for_heritage(CIV_HERITAGE_EASTERN);
    int south = country_name_count_for_heritage(CIV_HERITAGE_SOUTHERN);
    int north = country_name_count_for_heritage(CIV_HERITAGE_NORTHERN);
    int province_ok = province_name_count_for_heritage(CIV_HERITAGE_NORTHERN) ==
                      province_name_count_for_heritage(CIV_HERITAGE_WESTERN) &&
                      province_name_count_for_heritage(CIV_HERITAGE_SOUTHERN) ==
                      province_name_count_for_heritage(CIV_HERITAGE_EASTERN) &&
                      strcmp(province_name_localized_for_heritage(CIV_HERITAGE_NORTHERN, 0, 0),
                             province_name_localized_for_heritage(CIV_HERITAGE_WESTERN, 0, 0)) == 0 &&
                      strcmp(province_name_localized_for_heritage(CIV_HERITAGE_SOUTHERN, 0, 0),
                             province_name_localized_for_heritage(CIV_HERITAGE_EASTERN, 0, 0)) == 0;
    int labels_ok = strcmp(civilization_heritage_label(CIV_HERITAGE_WESTERN, 0), "Western") == 0 &&
                    strcmp(civilization_heritage_label(CIV_HERITAGE_EASTERN, 0), "Eastern") == 0 &&
                    strcmp(civilization_heritage_label(CIV_HERITAGE_SOUTHERN, 0), "Southern") == 0 &&
                    strcmp(civilization_heritage_label(CIV_HERITAGE_NORTHERN, 0), "Northern") == 0;
    ok &= west == 200 && east == 200 && south == 200 && north == 200 && province_ok && labels_ok;
    fprintf(file, "case=heritage_name_pools ok=%d country_counts=%d/%d/%d/%d province_reuse=%d labels=%d\n",
            ok, west, east, south, north, province_ok, labels_ok);
    for (int i = 0; i < 4; i++) {
        int total = i == 0 ? 4 : i == 1 ? 5 : i == 2 ? 6 : 26;
        count_heritage_queue(total, counts);
        distribution_all_ok &= distribution_ok(counts, total);
        fprintf(file, "case=heritage_distribution count=%d counts=%d/%d/%d/%d ok=%d\n",
                total, counts[0], counts[1], counts[2], counts[3],
                distribution_ok(counts, total));
    }
    return ok && distribution_all_ok;
}

int run_worldgen_probe(void) {
    WorldGenConfig config = probe_config();
    WorldGenContext *prepared_world;
    RoutePotentialStats route_stats;
    WorldGenProgress progress;
    unsigned long start_ms;
    unsigned long route_start_ms;
    FILE *file;
    int ok;

    CreateDirectoryA("logs", NULL);
    file = fopen("logs/worldgen_probe.txt", "w");
    if (!file) return 1;
    ok = check_heritage_setup(file);
    game_worldgen_hydrology_probe_reset();
    ok &= game_worldgen_landform_semantics_probe_run(file, &config);
    ok &= game_worldgen_lake_lifecycle_probe_run(file);
    ok &= game_worldgen_terrain_probe_run(file, &config);
    ok &= game_worldgen_mountain_shape_probe_run(file, &config);
    ok &= game_worldgen_physical_probe_run_matrix(file, &config);
    ok &= game_worldgen_commit_probe_run(file);
    ok &= game_worldgen_failure_probe_run(file);
    ok &= game_worldgen_snapshot_recovery_probe_run(file);
    ok &= game_worldgen_diagnostics_probe_run(file);
    ok &= game_worldgen_river_payload_probe_run(file);
    reset_probe_world();
    ok &= game_worldgen_region_water_probe_run_real_lake_fixture(file);
    /* Clear derived caches touched by the isolated city activation fixture. */
    reset_probe_world();
    worldgen_progress_begin();
    start_ms = GetTickCount();
    prepared_world = world_gen_prepare_with_config(&config);
    if (!prepared_world) {
        const WorldGenDiagnostics *diagnostics = world_gen_last_diagnostics();
        int truncated = diagnostics->river_segments_required -
                        diagnostics->river_segments_copied;
        fprintf(file, "case=physical_prepare label=map_size_extreme required_paths=%d "
                      "copied_paths=%d truncated=%d count_valid=%d ok=0\noverall_ok=0\n",
                diagnostics->river_segments_required, diagnostics->river_segments_copied,
                truncated, river_path_count_valid(diagnostics->river_segments_required,
                                                  MAP_W, MAP_H));
        fclose(file);
        return 1;
    }
    ok &= game_worldgen_physical_probe_check_context(file, "map_size_extreme",
                                                     prepared_world, 1, 0);
    ok &= game_worldgen_hydrology_probe_check_context(file, "map_size_extreme",
                                                      prepared_world, 0);
    {
        int commit_ok = world_gen_commit_prepared(prepared_world);
        world_gen_release_prepared(prepared_world);
        fprintf(file, "case=physical_matrix extreme=1 commit=%d ok=%d\n", commit_ok, commit_ok);
        if (!commit_ok) {
            fprintf(file, "overall_ok=0\n");
            fclose(file);
            return 1;
        }
    }
    ok &= game_worldgen_hydrology_probe_finish(file);
    world_generated = 1;
    ports_reset_regions();
    regions_generate(region_size_slider);
    ports_ensure_island_ports();
    world_invalidate_region_cache();
    simulation_seed_default_civilizations();
    ok &= game_worldgen_region_water_probe_run(file);
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
    ok &= game_worldgen_live_validation_probe_run(file, &config);
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
    {
        int placed_counts[CIV_HERITAGE_COUNT] = {0};
        for (int i = 0; i < civ_count; i++) if (civs[i].heritage >= 0 && civs[i].heritage < CIV_HERITAGE_COUNT)
            placed_counts[civs[i].heritage]++;
        ok &= distribution_ok(placed_counts, civ_count);
        fprintf(file, "case=heritage_distribution_generated civs=%d counts=%d/%d/%d/%d ok=%d\n",
                civ_count, placed_counts[0], placed_counts[1], placed_counts[2], placed_counts[3],
                distribution_ok(placed_counts, civ_count));
    }
    fprintf(file, "overall_ok=%d\n", ok);
    fclose(file);
    return ok ? 0 : 1;
}
