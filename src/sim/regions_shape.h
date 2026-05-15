#ifndef WORLD_SIM_REGIONS_SHAPE_H
#define WORLD_SIM_REGIONS_SHAPE_H

void regions_shape_refine(int target_size);
void regions_shape_repair_ugly(int target_size);
int regions_shape_last_merge_count(void);
int regions_shape_last_ugly_count(void);
int regions_shape_last_repair_tiles(void);
int regions_shape_last_max_aspect_before(void);
int regions_shape_last_max_aspect_after(void);

#endif
