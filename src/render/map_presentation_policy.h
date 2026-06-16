#ifndef WORLD_SIM_MAP_PRESENTATION_POLICY_H
#define WORLD_SIM_MAP_PRESENTATION_POLICY_H

#include "core/constants.h"

static inline int map_presentation_extreme_dimensions(int map_w, int map_h) {
    return map_w >= MAX_MAP_W && map_h >= MAX_MAP_H;
}

static inline int map_presentation_province_border_width(int map_w, int map_h,
                                                        int normal_width) {
    return map_presentation_extreme_dimensions(map_w, map_h) && normal_width < 2 ?
           2 : normal_width;
}

static inline int map_presentation_country_border_width(int map_w, int map_h,
                                                       int normal_width) {
    return map_presentation_extreme_dimensions(map_w, map_h) && normal_width < 3 ?
           3 : normal_width;
}

static inline int map_presentation_highlight_width_boost(int map_w, int map_h) {
    return map_presentation_extreme_dimensions(map_w, map_h) ? 1 : 0;
}

#endif
