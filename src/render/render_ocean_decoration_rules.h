#ifndef WORLD_SIM_RENDER_OCEAN_DECORATION_RULES_H
#define WORLD_SIM_RENDER_OCEAN_DECORATION_RULES_H

#include "render/render_ocean_assets.h"

typedef enum {
    OCEAN_MOTIF_WATER_DEEP_ONLY,
    OCEAN_MOTIF_WATER_SHALLOW_OR_DEEP
} OceanMotifWaterRule;

OceanMotifWaterRule ocean_decoration_motif_water_rule(const OceanMotifAssetInfo *info);
int ocean_decoration_motif_base_width(const OceanMotifAssetInfo *info);
int ocean_decoration_motif_scaled_footprint(const OceanMotifAssetInfo *info,
                                            int size, int vertical);
int ocean_decoration_exterior_spacing_norm(int same_type);
int ocean_decoration_interior_spacing_tiles(int same_type);

#endif
