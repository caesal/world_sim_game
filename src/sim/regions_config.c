#include "sim/regions.h"

static int clamp_local(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

int regions_target_size_from_slider(int value) {
    int v = clamp_local(value, 0, 100);
    return 90 + v * 4 + v * v * 630 / 10000;
}

int regions_estimated_count_for_settings(int width, int height, int ocean_percent,
                                         int region_size_value, int *cap_reached) {
    int target = regions_target_size_from_slider(region_size_value);
    int land_estimate = width * height * clamp_local(100 - ocean_percent, 5, 100) / 100;
    int count = land_estimate / (target > 0 ? target : 1);

    if (count < 1) count = 1;
    if (cap_reached) *cap_reached = count >= MAX_NATURAL_REGIONS;
    return clamp_local(count, 1, MAX_NATURAL_REGIONS);
}
