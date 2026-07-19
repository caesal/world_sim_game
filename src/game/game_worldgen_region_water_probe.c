#include "game/game_worldgen_region_water_probe.h"

#include "core/game_types.h"
#include "sim/regions.h"
#include "sim/regions_land_anchor.h"
#include "sim/regions_settlement.h"
#include "world/terrain_query.h"
#include "world/rivers.h"
#include "world/world_gen.h"

#include <stdlib.h>
#include <string.h>

static WorldGenConfig real_lake_fixture_config(void) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    config.seed = 731905u;
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

static int find_qualified_lake_land_pair(const WorldGenContext *context,
                                         int *lake_x, int *lake_y,
                                         int *land_x, int *land_y,
                                         int *lake_cells, int *semantic_errors) {
    static const int dx[4] = {1, 0, -1, 0};
    static const int dy[4] = {0, 1, 0, -1};
    int found = 0;
    int i;
    *lake_cells = 0;
    *semantic_errors = 0;
    for (i = 0; i < context->tile_count; i++) {
        int flagged = (context->river_flags[i] & WORLD_GEN_RIVER_LAKE) != 0;
        int semantic = context->geography[i] == GEO_LAKE;
        int direction;
        if (flagged != semantic) (*semantic_errors)++;
        if (!flagged) continue;
        (*lake_cells)++;
        if (found) continue;
        for (direction = 0; direction < 4; direction++) {
            int x = i % context->width;
            int y = i / context->width;
            int nx = x + dx[direction];
            int ny = y + dy[direction];
            int neighbor;
            if (nx < 0 || nx >= context->width ||
                ny < 0 || ny >= context->height) continue;
            neighbor = ny * context->width + nx;
            if (!is_land((Geography)context->geography[neighbor])) continue;
            *lake_x = x;
            *lake_y = y;
            *land_x = nx;
            *land_y = ny;
            found = 1;
            break;
        }
    }
    return found;
}

int game_worldgen_region_water_probe_run_real_lake_fixture(FILE *file) {
    WorldGenConfig config = real_lake_fixture_config();
    WorldGenContext *context = NULL;
    const RiverNetworkView *view;
    Tile saved_lake_tile;
    Tile saved_land_tile;
    NaturalRegion saved_region;
    City saved_city;
    int lake_x = -1;
    int lake_y = -1;
    int land_x = -1;
    int land_y = -1;
    int anchor_x = -1;
    int anchor_y = -1;
    int lake_cells = 0;
    int semantic_errors = 0;
    int qualified_components = 0;
    int old_region_count = region_count;
    int old_city_count = city_count;
    int fixture_region = old_region_count;
    int fixture_city = -1;
    int prepared = 0;
    int qualified = 0;
    int anchor_ok = 0;
    int city_ok = 0;
    int restored = 0;
    int state_patched = 0;
    int ok;
    if (!file || fixture_region < 0 || fixture_region >= MAX_NATURAL_REGIONS ||
        old_city_count < 0 || old_city_count >= MAX_CITIES) return 0;
    context = world_gen_prepare_for_dimensions(&config, MAX_MAP_W, MAX_MAP_H);
    prepared = context != NULL;
    view = river_network_latest_view();
    if (!context || !view || !river_network_view_matches_context(context)) goto done;
    qualified_components = view->diagnostics.lake_qualified_components;
    qualified = find_qualified_lake_land_pair(
        context, &lake_x, &lake_y, &land_x, &land_y,
        &lake_cells, &semantic_errors) && qualified_components > 0 &&
        lake_cells == view->diagnostics.lake_cells && semantic_errors == 0;
    if (!qualified || lake_x >= MAP_W || lake_y >= MAP_H ||
        land_x >= MAP_W || land_y >= MAP_H) goto done;

    saved_lake_tile = world[lake_y][lake_x];
    saved_land_tile = world[land_y][land_x];
    saved_region = natural_regions[fixture_region];
    saved_city = cities[old_city_count];
    state_patched = 1;
    world[lake_y][lake_x].geography = GEO_LAKE;
    world[lake_y][lake_x].region_id = -1;
    world[land_y][land_x].geography =
        (Geography)context->geography[land_y * context->width + land_x];
    world[land_y][land_x].region_id = fixture_region;
    memset(&natural_regions[fixture_region], 0,
           sizeof(natural_regions[fixture_region]));
    natural_regions[fixture_region].id = fixture_region;
    natural_regions[fixture_region].alive = 1;
    natural_regions[fixture_region].tile_count = 1;
    natural_regions[fixture_region].owner_civ = -1;
    natural_regions[fixture_region].city_id = -1;
    natural_regions[fixture_region].center_x = lake_x;
    natural_regions[fixture_region].center_y = lake_y;
    natural_regions[fixture_region].capital_x = land_x;
    natural_regions[fixture_region].capital_y = land_y;
    region_count = fixture_region + 1;

    anchor_ok = regions_land_anchor_find_nearest_member(
        fixture_region, lake_x, lake_y, &anchor_x, &anchor_y) &&
        anchor_x >= 0 && anchor_x < MAP_W && anchor_y >= 0 && anchor_y < MAP_H &&
        world[anchor_y][anchor_x].region_id == fixture_region &&
        is_land(world[anchor_y][anchor_x].geography) &&
        world[anchor_y][anchor_x].geography != GEO_LAKE &&
        abs(anchor_x - lake_x) + abs(anchor_y - lake_y) == 1;
    if (anchor_ok) {
        natural_regions[fixture_region].center_x = anchor_x;
        natural_regions[fixture_region].center_y = anchor_y;
        natural_regions[fixture_region].capital_x = anchor_x;
        natural_regions[fixture_region].capital_y = anchor_y;
        fixture_city = regions_ensure_local_city_slot(fixture_region);
        if (fixture_city >= 0) {
            int active_city = regions_activate_local_city(
                fixture_region, 0, 100, 0, 0);
            city_ok = fixture_city == old_city_count &&
                active_city == fixture_city && cities[fixture_city].alive &&
                cities[fixture_city].x == anchor_x &&
                cities[fixture_city].y == anchor_y &&
                abs(cities[fixture_city].x - lake_x) +
                    abs(cities[fixture_city].y - lake_y) == 1 &&
                is_land(world[cities[fixture_city].y][cities[fixture_city].x].geography) &&
                world[cities[fixture_city].y][cities[fixture_city].x].geography != GEO_LAKE &&
                regions_city_is_local_to_region(fixture_city, fixture_region);
        }
    }

done:
    if (state_patched) {
        cities[old_city_count] = saved_city;
        city_count = old_city_count;
        natural_regions[fixture_region] = saved_region;
        region_count = old_region_count;
        world[lake_y][lake_x] = saved_lake_tile;
        world[land_y][land_x] = saved_land_tile;
        restored = city_count == old_city_count && region_count == old_region_count &&
            memcmp(&cities[old_city_count], &saved_city, sizeof(saved_city)) == 0 &&
            memcmp(&natural_regions[fixture_region], &saved_region,
                   sizeof(saved_region)) == 0 &&
            memcmp(&world[lake_y][lake_x], &saved_lake_tile,
                   sizeof(saved_lake_tile)) == 0 &&
            memcmp(&world[land_y][land_x], &saved_land_tile,
                   sizeof(saved_land_tile)) == 0;
    }
    ok = prepared && qualified && anchor_ok && city_ok && restored;
    fprintf(file,
            "case=region_city_real_lake_fixture seed=%u prepared=%d "
            "qualified_components=%d lake_cells=%d semantic_errors=%d "
            "lake=%d,%d anchor=%d,%d city=%d city_on_land=%d "
            "city_adjacent=%d restored=%d ok=%d\n",
            config.seed, prepared, qualified_components, lake_cells,
            semantic_errors, lake_x, lake_y, anchor_x, anchor_y, fixture_city,
            city_ok, city_ok, restored, ok);
    world_gen_release_prepared(context);
    return ok;
}

static int valid_point(int x, int y) {
    return x >= 0 && x < MAP_W && y >= 0 && y < MAP_H;
}

static int point_is_lake(int x, int y) {
    return valid_point(x, y) && world[y][x].geography == GEO_LAKE;
}

static int nearest_anchor_fixture(FILE *file) {
    RegionsLandAnchorSearch search;
    int nearest_x = -1;
    int nearest_y = -1;
    int tie_x = -1;
    int tie_y = -1;
    int nearest_ok;
    int tie_ok;

    regions_land_anchor_search_begin(&search, 7, 10, 10);
    regions_land_anchor_search_consider(&search, 7, 1, 15, 10);
    regions_land_anchor_search_consider(&search, 6, 1, 10, 10);
    regions_land_anchor_search_consider(&search, 7, 0, 10, 10);
    regions_land_anchor_search_consider(&search, 7, 1, 11, 10);
    regions_land_anchor_search_consider(&search, 7, 1, 8, 10);
    nearest_ok = regions_land_anchor_search_result(&search, &nearest_x, &nearest_y) &&
                 nearest_x == 11 && nearest_y == 10;
    regions_land_anchor_search_begin(&search, 7, 10, 10);
    regions_land_anchor_search_consider(&search, 7, 1, 10, 11);
    regions_land_anchor_search_consider(&search, 7, 1, 11, 10);
    regions_land_anchor_search_consider(&search, 7, 1, 9, 10);
    regions_land_anchor_search_consider(&search, 7, 1, 10, 9);
    tie_ok = regions_land_anchor_search_result(&search, &tie_x, &tie_y) &&
             tie_x == 10 && tie_y == 9;
    fprintf(file,
            "case=region_lake_anchor_nearest_tie nearest=%d,%d tie=%d,%d "
            "wrong_region_ignored=1 nonland_ignored=1 stable_order=y_then_x ok=%d\n",
            nearest_x, nearest_y, tie_x, tie_y, nearest_ok && tie_ok);
    return nearest_ok && tie_ok;
}

int game_worldgen_region_water_probe_run(FILE *file) {
    int region_center_invalid = 0;
    int region_center_lake_errors = 0;
    int region_capital_invalid = 0;
    int region_capital_lake_errors = 0;
    int city_center_errors = 0;
    int city_center_lake_errors = 0;
    int city_region_errors = 0;
    int civ_capital_errors = 0;
    int civ_capital_lake_errors = 0;
    int city_adjacent_lake_edges = 0;
    int anchor_ok;
    int nonempty;
    int i;
    if (!file) return 0;
    anchor_ok = nearest_anchor_fixture(file);
    for (i = 0; i < region_count; i++) {
        const NaturalRegion *region = &natural_regions[i];
        if (!region->alive || region->tile_count <= 0) continue;
        if (!valid_point(region->center_x, region->center_y)) region_center_invalid++;
        if (point_is_lake(region->center_x, region->center_y)) region_center_lake_errors++;
        if (!valid_point(region->capital_x, region->capital_y)) region_capital_invalid++;
        if (point_is_lake(region->capital_x, region->capital_y)) region_capital_lake_errors++;
    }
    for (i = 0; i < city_count; i++) {
        const City *city = &cities[i];
        int dx;
        int dy;
        if (!city->alive) continue;
        if (point_is_lake(city->x, city->y)) city_center_lake_errors++;
        if (city->x < 0 || city->x >= MAP_W || city->y < 0 || city->y >= MAP_H ||
            !is_land(world[city->y][city->x].geography)) {
            city_center_errors++;
            continue;
        }
        {
            int region_id = world[city->y][city->x].region_id;
            if (region_id < 0 || region_id >= region_count ||
                !natural_regions[region_id].alive ||
                regions_region_for_city(i) != region_id) city_region_errors++;
        }
        for (dy = -1; dy <= 1; dy++) {
            for (dx = -1; dx <= 1; dx++) {
                int x = city->x + dx;
                int y = city->y + dy;
                if ((dx == 0 && dy == 0) || x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) continue;
                if (world[y][x].geography == GEO_LAKE) city_adjacent_lake_edges++;
            }
        }
    }
    for (i = 0; i < civ_count; i++) {
        int city_id;
        if (!civs[i].alive) continue;
        city_id = civs[i].capital_city;
        if (city_id >= 0 && city_id < city_count && cities[city_id].alive &&
            point_is_lake(cities[city_id].x, cities[city_id].y)) {
            civ_capital_lake_errors++;
        }
        if (city_id < 0 || city_id >= city_count || !cities[city_id].alive ||
            cities[city_id].owner != i ||
            cities[city_id].x < 0 || cities[city_id].x >= MAP_W ||
            cities[city_id].y < 0 || cities[city_id].y >= MAP_H ||
            !is_land(world[cities[city_id].y][cities[city_id].x].geography)) {
            civ_capital_errors++;
        }
    }
    nonempty = region_count > 0 && city_count > 0 && civ_count > 0;
    fprintf(file,
            "case=region_city_lake_centers regions=%d cities=%d civs=%d "
            "nonempty=%d anchor_ok=%d region_center_invalid=%d region_center_lake_errors=%d "
            "region_capital_invalid=%d region_capital_lake_errors=%d "
            "city_center_errors=%d city_center_lake_errors=%d city_region_errors=%d "
            "civ_capital_errors=%d civ_capital_lake_errors=%d city_adjacent_lake_edges=%d ok=%d\n",
            region_count, city_count, civ_count, nonempty, anchor_ok, region_center_invalid,
            region_center_lake_errors, region_capital_invalid, region_capital_lake_errors,
            city_center_errors, city_center_lake_errors, city_region_errors,
            civ_capital_errors, civ_capital_lake_errors,
            city_adjacent_lake_edges,
            nonempty && anchor_ok && region_center_invalid == 0 &&
            region_center_lake_errors == 0 && region_capital_invalid == 0 &&
            region_capital_lake_errors == 0 && city_center_errors == 0 &&
            city_center_lake_errors == 0 && city_region_errors == 0 &&
            civ_capital_errors == 0 && civ_capital_lake_errors == 0);
    return nonempty && anchor_ok && region_center_invalid == 0 &&
           region_center_lake_errors == 0 && region_capital_invalid == 0 &&
           region_capital_lake_errors == 0 && city_center_errors == 0 &&
           city_center_lake_errors == 0 && city_region_errors == 0 &&
           civ_capital_errors == 0 && civ_capital_lake_errors == 0;
}
