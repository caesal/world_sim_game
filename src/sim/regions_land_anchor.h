#ifndef WORLD_SIM_REGIONS_LAND_ANCHOR_H
#define WORLD_SIM_REGIONS_LAND_ANCHOR_H

typedef struct {
    int region_id;
    int origin_x;
    int origin_y;
    int best_x;
    int best_y;
    long long best_distance_sq;
    int found;
} RegionsLandAnchorSearch;

void regions_land_anchor_search_begin(RegionsLandAnchorSearch *search,
                                      int region_id, int origin_x, int origin_y);
void regions_land_anchor_search_consider(RegionsLandAnchorSearch *search,
                                         int candidate_region_id, int candidate_is_land,
                                         int x, int y);
int regions_land_anchor_search_result(const RegionsLandAnchorSearch *search, int *x, int *y);
int regions_land_anchor_find_nearest_member(int region_id, int origin_x, int origin_y,
                                            int *x, int *y);

#endif
