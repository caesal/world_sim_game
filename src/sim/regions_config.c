#include "sim/regions.h"

#include "core/constants.h"

static int clamp_local(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

int regions_target_size_from_slider(int value) {
    int v = clamp_local(value, 0, 100);
    return 90 + v * 4 + v * v * 630 / 10000;
}

int regions_max_count_for_map_size(int map_size) {
    if (map_size == MAP_SIZE_SMALL) return 512;
    if (map_size == MAP_SIZE_LARGE) return 1024;
    if (map_size == MAP_SIZE_EXTREME) return 1536;
    return 768;
}

int regions_max_count_for_dimensions(int width, int height) {
    if (width >= 1152 || height >= 800) return regions_max_count_for_map_size(MAP_SIZE_EXTREME);
    if (width >= 864 || height >= 600) return regions_max_count_for_map_size(MAP_SIZE_LARGE);
    if (width >= 720 || height >= 500) return regions_max_count_for_map_size(MAP_SIZE_MEDIUM);
    return regions_max_count_for_map_size(MAP_SIZE_SMALL);
}

int regions_generation_cap(void) {
    return regions_max_count_for_dimensions(MAP_W, MAP_H);
}

int regions_estimated_count_for_settings(int width, int height, int ocean_percent,
                                         int region_size_value, int *cap_reached) {
    int target = regions_target_size_from_slider(region_size_value);
    int cap = regions_max_count_for_dimensions(width, height);
    int land_estimate = width * height * clamp_local(100 - ocean_percent, 5, 100) / 100;
    int count = land_estimate / (target > 0 ? target : 1);

    if (count < 1) count = 1;
    if (cap_reached) *cap_reached = count >= cap;
    return clamp_local(count, 1, cap);
}
