#ifndef WORLD_SIM_REGIONS_VALIDATE_H
#define WORLD_SIM_REGIONS_VALIDATE_H

typedef struct {
    int target_size;
    int tiny_regions;
    int huge_regions;
    int thin_regions;
    int disconnected_regions;
    int tiny_merged;
    int huge_split;
    int disconnected_reassigned;
    int sliver_smoothed;
    int cap_reached;
} RegionValidationStats;

void regions_validate_postprocess(int target_size);
const RegionValidationStats *regions_validate_last_stats(void);

#endif
