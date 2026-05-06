#ifndef WORLD_SIM_REGION_BOUNDARY_H
#define WORLD_SIM_REGION_BOUNDARY_H

int region_boundary_crossing_cost(int x, int y, int nx, int ny);
int region_boundary_seed_score(int x, int y, int seed_count, const int *seed_x, const int *seed_y);
int region_boundary_compactness_penalty(int seed_x, int seed_y, int x, int y, int target_size);
void region_boundary_debug_summary(void);

#endif
