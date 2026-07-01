#ifndef WORLD_SIM_RENDER_OCEAN_ASSETS_H
#define WORLD_SIM_RENDER_OCEAN_ASSETS_H

#include <windows.h>

typedef struct {
    const char *id;
    int default_w;
    int default_h;
    int footprint_w;
    int footprint_h;
    int min_coast_clear_tiles;
    int min_label_clear_px;
    int allow_interior;
    int allow_exterior;
    int weight;
    int mirror_allowed;
} OceanMotifAssetInfo;

int ocean_assets_draw_texture(HDC hdc, RECT rect);
int ocean_assets_texture_ready(void);
int ocean_assets_motif_count(void);
const OceanMotifAssetInfo *ocean_assets_motif_info(int index);
int ocean_assets_motifs_ready(void);
int ocean_assets_draw_motif(HDC hdc, int index, RECT dst);

#endif
