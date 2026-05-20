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
    int final_region_count;
    int average_region_size;
    int largest_region_size;
    int smallest_region_size;
    int worst_elongation;
    int ribbon_regions;
    int low_fill_regions;
    int artificial_diagonal_regions;
    int regions_resplit;
    int regions_merged_for_shape;
    int regions_repaired_by_local_regrow;
    int worst_fill_percent;
    int worst_perimeter_area;
} RegionValidationStats;

void regions_validate_postprocess(int target_size);
void regions_validate_light_postprocess(int target_size);
const RegionValidationStats *regions_validate_last_stats(void);

#endif
