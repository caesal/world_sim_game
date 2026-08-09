#include "game/game_decision_topology_probe.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "game/game_worldgen.h"
#include "sim/diplomacy.h"
#include "sim/expansion.h"
#include "sim/expansion_land_topology.h"
#include "sim/maritime.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/route_potential.h"
#include "sim/simulation.h"
#include "sim/war.h"
#include "world/ports.h"
#include "world/world_gen.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#define OUTPUT_DIR \
    "build/validation/decision_cache_monthly_coherence_20260801/02_topology_differential/attempt_02_shared_production"
#define OUTPUT_SUMMARY OUTPUT_DIR "/summary.txt"

static int claimable(const NaturalRegion *region) {
    int owner;
    if (!region || !region->alive) return 0;
    owner = region->owner_civ;
    return owner < 0 || owner >= civ_count || !civs[owner].alive;
}

static int legacy_near_path(int region_id, int civ_id) {
    const NaturalRegion *region = regions_get(region_id);
    int i;
    if (!claimable(region)) return 0;
    for (i = 0; i < region->neighbor_count; i++) {
        int neighbor = region->neighbors[i];
        const NaturalRegion *near_region;
        int j;
        if (neighbor < 0 || neighbor >= region_count) continue;
        near_region = regions_get(neighbor);
        if (!near_region || !near_region->alive) continue;
        if (near_region->owner_civ == civ_id) return 1;
        if (!claimable(near_region)) continue;
        for (j = 0; j < near_region->neighbor_count; j++) {
            int next = near_region->neighbors[j];
            if (next >= 0 && next < region_count &&
                natural_regions[next].owner_civ == civ_id) return 1;
        }
    }
    return 0;
}

static ExpansionLandTopologyCounts legacy_counts(int civ_id) {
    ExpansionLandTopologyCounts counts = {0};
    int i;
    for (i = 0; i < region_count; i++) {
        const NaturalRegion *region = regions_get(i);
        if (region) counts.viable_regions++;
        if (!claimable(region)) continue;
        counts.global_unowned_regions++;
        if (regions_region_has_owner_neighbor(i, civ_id)) {
            counts.land_adjacent_unowned_regions++;
        } else if (legacy_near_path(i, civ_id)) {
            counts.land_nearby_unowned_regions++;
        }
    }
    return counts;
}

static int compare_one(FILE *file, const char *fixture, int civ_id) {
    ExpansionLandTopologyCounts legacy = legacy_counts(civ_id);
    ExpansionLandTopologyCounts shared;
    ExpansionAIDiagnostics production = expansion_ai_diagnostics(civ_id, 0);
    int shared_ok = expansion_land_topology_counts(civ_id, &shared);
    int percent = legacy.viable_regions > 0 ?
        legacy.global_unowned_regions * 100 / legacy.viable_regions : 0;
    int ok = shared_ok &&
        memcmp(&legacy, &shared, sizeof(legacy)) == 0 &&
        production.global_unowned_regions == legacy.global_unowned_regions &&
        production.land_adjacent_unowned_regions == legacy.land_adjacent_unowned_regions &&
        production.land_nearby_unowned_regions == legacy.land_nearby_unowned_regions &&
        production.global_unowned_percent == percent &&
        production.adjacent_unowned_regions == legacy.land_adjacent_unowned_regions &&
        production.nearby_unowned_regions == legacy.land_adjacent_unowned_regions +
                                              legacy.land_nearby_unowned_regions;
    fprintf(file,
            "fixture=%s civ=%d ok=%d viable=%d global=%d direct=%d near=%d percent=%d production=%d/%d/%d/%d\n",
            fixture, civ_id, ok, legacy.viable_regions, legacy.global_unowned_regions,
            legacy.land_adjacent_unowned_regions, legacy.land_nearby_unowned_regions,
            percent, production.global_unowned_regions,
            production.land_adjacent_unowned_regions,
            production.land_nearby_unowned_regions,
            production.global_unowned_percent);
    return ok;
}

static void add_neighbor(int from, int to) {
    NaturalRegion *region = &natural_regions[from];
    if (region->neighbor_count < MAX_REGION_NEIGHBORS) {
        region->neighbors[region->neighbor_count++] = to;
    }
}

static void region(int id, int alive, int tiles, int owner) {
    NaturalRegion *value = &natural_regions[id];
    memset(value, 0, sizeof(*value));
    value->id = id;
    value->alive = alive;
    value->tile_count = tiles;
    value->owner_civ = owner;
    value->capital_x = tiles > 0 ? id : -1;
    value->capital_y = tiles > 0 ? 0 : -1;
}

static void setup_pathological_fixture(void) {
    int i;
    simulation_reset_state();
    world_generated = 1;
    civ_count = 3;
    memset(civs, 0, sizeof(civs));
    for (i = 0; i < civ_count; i++) {
        civs[i].uid = 7000 + i;
        civs[i].alive = i != 2;
        civs[i].population = 10000;
        civs[i].governance = civs[i].cohesion = 8;
        civs[i].logistics = civs[i].commerce = 5;
        civs[i].capital_city = -1;
    }
    region_count = 11;
    memset(natural_regions, 0, sizeof(natural_regions));
    region(0, 1, 5, 0);
    region(1, 1, 5, -1);
    region(2, 1, 5, -1);
    region(3, 1, 5, 1);
    region(4, 1, 5, 2);
    region(5, 1, 5, 99);
    region(6, 0, 5, -1);
    region(7, 1, 5, -1);
    region(8, 0, 0, 0);
    region(9, 1, 5, -1);
    region(10, 1, 5, 0);
    add_neighbor(1, 0); add_neighbor(1, 0); add_neighbor(1, 99);
    add_neighbor(2, 7); add_neighbor(7, 0); add_neighbor(7, 8);
    add_neighbor(5, 3); add_neighbor(5, 7);
    add_neighbor(7, 10);
    add_neighbor(9, 4); add_neighbor(4, 0);
    dirty_mark_territory();
    dirty_mark_civ();
    expansion_land_topology_reset();
}

static WorldGenConfig generated_config(void) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    config.seed = 4400341u;
    config.random_seed = 0;
    config.ocean = 45; config.continent = 60; config.relief = 56;
    config.moisture = 48; config.drought = 50; config.vegetation = 54;
    config.bias_forest = 58; config.bias_desert = 38;
    config.bias_mountain = 60; config.bias_wetland = 42;
    return config;
}

static int setup_generated_fixture(void) {
    WorldGenConfig config = generated_config();
    pending_map_size = MAP_SIZE_EXTREME;
    initial_civ_count = 26;
    region_size_slider = 34;
    set_active_map_size(pending_map_size);
    diplomacy_reset(); war_reset(); simulation_reset_state(); game_clear_world_tiles();
    world_generated = 0;
    generate_world_with_config(&config);
    world_generated = 1;
    ports_reset_regions(); regions_generate(region_size_slider); ports_ensure_island_ports();
    world_invalidate_region_cache(); simulation_seed_default_civilizations();
    world_recalculate_territory(); ports_ensure_island_ports(); ports_refresh_city_regions();
    route_potential_rebuild(); maritime_rebuild_routes(); diplomacy_update_contacts();
    expansion_land_topology_reset();
    return civ_count == 26 && region_count > 600;
}

static int check_cache_revisions(FILE *file) {
    ExpansionLandTopologyCounts counts;
    ExpansionLandTopologyStatus a, b, c, d;
    int i;
    expansion_land_topology_reset();
    for (i = 0; i < civ_count; i++) if (civs[i].alive) expansion_land_topology_counts(i, &counts);
    expansion_land_topology_status(&a);
    for (i = 0; i < civ_count; i++) if (civs[i].alive) expansion_land_topology_counts(i, &counts);
    expansion_land_topology_status(&b);
    dirty_mark_territory();
    for (i = 0; i < civ_count; i++) if (civs[i].alive) expansion_land_topology_counts(i, &counts);
    expansion_land_topology_status(&c);
    dirty_mark_civ();
    for (i = 0; i < civ_count; i++) if (civs[i].alive) expansion_land_topology_counts(i, &counts);
    expansion_land_topology_status(&d);
    fprintf(file,
            "case=revision_cache ok=%d traversals=%llu/%llu/%llu/%llu hits=%llu regions=%llu rebuild_us=%llu\n",
            a.traversal_count == 1 && b.traversal_count == 1 &&
            c.traversal_count == 2 && d.traversal_count == 3,
            (unsigned long long)a.traversal_count, (unsigned long long)b.traversal_count,
            (unsigned long long)c.traversal_count, (unsigned long long)d.traversal_count,
            (unsigned long long)d.cache_hit_count, (unsigned long long)d.regions_visited,
            (unsigned long long)d.last_rebuild_us);
    return a.traversal_count == 1 && b.traversal_count == 1 &&
           c.traversal_count == 2 && d.traversal_count == 3;
}

int run_decision_topology_probe(void) {
    FILE *file;
    int ok = 1;
    int i;
    CreateDirectoryA("build", NULL);
    CreateDirectoryA("build/validation", NULL);
    CreateDirectoryA("build/validation/decision_cache_monthly_coherence_20260801", NULL);
    CreateDirectoryA("build/validation/decision_cache_monthly_coherence_20260801/02_topology_differential", NULL);
    CreateDirectoryA(OUTPUT_DIR, NULL);
    file = fopen(OUTPUT_SUMMARY, "w");
    if (!file) return 2;
    setup_pathological_fixture();
    for (i = 0; i < civ_count; i++) if (civs[i].alive) ok &= compare_one(file, "pathological", i);
    ok &= check_cache_revisions(file);
    ok &= setup_generated_fixture();
    for (i = 0; i < civ_count; i++) if (civs[i].alive) ok &= compare_one(file, "generated", i);
    ok &= check_cache_revisions(file);
    fprintf(file, "generated_civs=%d generated_regions=%d overall_ok=%d\n",
            civ_count, region_count, ok);
    fclose(file);
    printf("decision topology summary: %s\n", OUTPUT_SUMMARY);
    return ok ? 0 : 1;
}
