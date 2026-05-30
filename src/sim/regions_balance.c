#include "sim/regions_balance.h"

#include <stdlib.h>

static int imax(int a, int b) { return a > b ? a : b; }

RegionSizeBand regions_size_band(int target_size) {
    RegionSizeBand band;
    target_size = imax(1, target_size);
    band.hard_min = imax(24, target_size * 30 / 100);
    band.soft_min = imax(40, target_size * 45 / 100);
    band.soft_max = imax(target_size + 120, target_size * 170 / 100);
    band.hard_max = imax(target_size + 180, target_size * 210 / 100);
    return band;
}

int regions_merge_target_score(int from_size, int neighbor_size, int target_size,
                               RegionSizeBand band, int force_merge, int boundary_score) {
    int combined = from_size + neighbor_size;
    int score = boundary_score;
    int miss = abs(combined - target_size);

    if (!force_merge && combined > band.soft_max) return -1000000;
    score -= miss / 2;
    if (combined < band.soft_min) score -= (band.soft_min - combined) * 2;
    if (combined > band.soft_max) score -= (combined - band.soft_max) * (force_merge ? 3 : 10);
    if (combined > band.hard_max) score -= (combined - band.hard_max) * 8;
    return score;
}
