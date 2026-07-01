#include "render/render_ocean_decoration_rules.h"

#include <string.h>

static int imax(int a, int b) { return a > b ? a : b; }

static int motif_id_is(const OceanMotifAssetInfo *info, const char *id) {
    return info && info->id && strcmp(info->id, id) == 0;
}

OceanMotifWaterRule ocean_decoration_motif_water_rule(const OceanMotifAssetInfo *info) {
    if (motif_id_is(info, "sailing_ship") ||
        motif_id_is(info, "wave_cluster") ||
        motif_id_is(info, "flying_fish")) {
        return OCEAN_MOTIF_WATER_SHALLOW_OR_DEEP;
    }
    return OCEAN_MOTIF_WATER_DEEP_ONLY;
}

int ocean_decoration_motif_base_width(const OceanMotifAssetInfo *info) {
    int base = info && info->default_w > 0 ? info->default_w : 96;
    return imax(22, base * 9 / 20);
}

int ocean_decoration_motif_scaled_footprint(const OceanMotifAssetInfo *info,
                                            int size, int vertical) {
    int base = vertical ? (info ? info->footprint_h : size) :
                          (info ? info->footprint_w : size);
    int def = info && info->default_w > 0 ? info->default_w : imax(1, size);
    return imax(16, (int)((long long)base * size / def));
}

int ocean_decoration_exterior_spacing_norm(int same_type) {
    return same_type ? 1180 : 680;
}

int ocean_decoration_interior_spacing_tiles(int same_type) {
    return same_type ? 18 : 9;
}
