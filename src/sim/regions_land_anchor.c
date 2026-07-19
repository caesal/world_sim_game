#include "sim/regions_land_anchor.h"

#include "core/game_types.h"
#include "world/terrain_query.h"

void regions_land_anchor_search_begin(RegionsLandAnchorSearch *search,
                                      int region_id, int origin_x, int origin_y) {
    if (!search) return;
    search->region_id = region_id;
    search->origin_x = origin_x;
    search->origin_y = origin_y;
    search->best_x = -1;
    search->best_y = -1;
    search->best_distance_sq = 0;
    search->found = 0;
}

void regions_land_anchor_search_consider(RegionsLandAnchorSearch *search,
                                         int candidate_region_id, int candidate_is_land,
                                         int x, int y) {
    long long dx;
    long long dy;
    long long distance_sq;

    if (!search || !candidate_is_land || candidate_region_id != search->region_id) return;
    dx = x - search->origin_x;
    dy = y - search->origin_y;
    distance_sq = dx * dx + dy * dy;
    if (search->found && (distance_sq > search->best_distance_sq ||
        (distance_sq == search->best_distance_sq &&
         (y > search->best_y || (y == search->best_y && x >= search->best_x))))) return;
    search->best_x = x;
    search->best_y = y;
    search->best_distance_sq = distance_sq;
    search->found = 1;
}

int regions_land_anchor_search_result(const RegionsLandAnchorSearch *search, int *x, int *y) {
    if (!search || !search->found || !x || !y) return 0;
    *x = search->best_x;
    *y = search->best_y;
    return 1;
}

int regions_land_anchor_find_nearest_member(int region_id, int origin_x, int origin_y,
                                            int *x, int *y) {
    RegionsLandAnchorSearch search;
    int candidate_x;
    int candidate_y;

    if (region_id < 0 || region_id >= MAX_NATURAL_REGIONS) return 0;
    regions_land_anchor_search_begin(&search, region_id, origin_x, origin_y);
    for (candidate_y = 0; candidate_y < MAP_H; candidate_y++) {
        for (candidate_x = 0; candidate_x < MAP_W; candidate_x++) {
            regions_land_anchor_search_consider(
                &search, world[candidate_y][candidate_x].region_id,
                is_land(world[candidate_y][candidate_x].geography), candidate_x, candidate_y);
        }
    }
    return regions_land_anchor_search_result(&search, x, y);
}
