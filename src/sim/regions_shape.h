#ifndef WORLD_SIM_REGIONS_SHAPE_H
#define WORLD_SIM_REGIONS_SHAPE_H

typedef enum {
    REGION_SHAPE_OK = 0,
    REGION_SHAPE_TINY,
    REGION_SHAPE_HUGE,
    REGION_SHAPE_RIBBON,
    REGION_SHAPE_LOW_FILL,
    REGION_SHAPE_DISCONNECTED,
    REGION_SHAPE_SLIVER,
    REGION_SHAPE_ARTIFICIAL_DIAGONAL
} RegionShapeClass;

void regions_shape_refine(int target_size);
void regions_shape_repair_ugly(int target_size);
int regions_shape_last_merge_count(void);
int regions_shape_last_ugly_count(void);
int regions_shape_last_repair_tiles(void);
int regions_shape_last_max_aspect_before(void);
int regions_shape_last_max_aspect_after(void);
int regions_shape_last_ribbon_count(void);
int regions_shape_last_low_fill_count(void);
int regions_shape_last_diagonal_count(void);
int regions_shape_last_resplit_count(void);
int regions_shape_last_shape_merge_count(void);
int regions_shape_last_local_regrow_count(void);
int regions_shape_last_worst_fill_percent(void);
int regions_shape_last_worst_perimeter_area(void);

#endif
