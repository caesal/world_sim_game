#ifndef WORLD_SIM_RENDER_OCEAN_ASSETS_H
#define WORLD_SIM_RENDER_OCEAN_ASSETS_H

#include <stdint.h>
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

typedef struct {
    uint64_t texture_decode_attempts;
    uint64_t texture_raster_calls;
    uint64_t texture_tile_draw_calls;
    uint64_t texture_raster_failures;
    uint64_t motif_decode_attempts;
    uint64_t motif_draw_calls;
    uint64_t motif_draw_failures;
} OceanAssetDebugStats;

typedef struct {
    int index;
    RECT dst;
} OceanMotifRasterItem;

extern OceanAssetDebugStats ocean_asset_debug_stats;

int ocean_assets_draw_texture(HDC hdc, RECT rect);
int ocean_assets_draw_texture_tiled(HDC hdc, RECT rect, int tile_px);
int ocean_assets_draw_texture_tiled_loaded(HDC hdc, RECT rect, int tile_px);
int ocean_assets_texture_tile_px(void);
/* The ready/count/info calls below may initialize assets; use from ensure/prewarm. */
int ocean_assets_texture_ready(void);
int ocean_assets_motif_count(void);
const OceanMotifAssetInfo *ocean_assets_motif_info(int index);
int ocean_assets_motifs_ready(void);
/* Loaded-state calls are side-effect-free and safe for presentation checks. */
int ocean_assets_texture_loaded(void);
int ocean_assets_manifest_loaded(void);
int ocean_assets_motif_count_loaded(void);
const OceanMotifAssetInfo *ocean_assets_motif_info_loaded(int index);
int ocean_assets_motifs_loaded(void);
int ocean_assets_draw_motif(HDC hdc, int index, RECT dst);
int ocean_assets_draw_motif_layer(uint32_t *pixels, int width, int height,
                                  const OceanMotifRasterItem *items, int count);
unsigned int ocean_assets_motif_identity(void);
OceanAssetDebugStats ocean_assets_debug_stats(void);
/* Resets counters only; decoded texture and motif assets remain loaded. */
void ocean_assets_reset_debug(void);

#endif
