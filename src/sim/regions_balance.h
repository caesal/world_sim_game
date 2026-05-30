#ifndef WORLD_SIM_REGIONS_BALANCE_H
#define WORLD_SIM_REGIONS_BALANCE_H

typedef struct {
    int hard_min;
    int soft_min;
    int soft_max;
    int hard_max;
} RegionSizeBand;

RegionSizeBand regions_size_band(int target_size);
int regions_merge_target_score(int from_size, int neighbor_size, int target_size,
                               RegionSizeBand band, int force_merge, int boundary_score);

#endif
