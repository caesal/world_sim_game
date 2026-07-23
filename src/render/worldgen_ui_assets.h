#ifndef WORLD_SIM_WORLDGEN_UI_ASSETS_H
#define WORLD_SIM_WORLDGEN_UI_ASSETS_H

#include <windows.h>

#define WORLDGEN_UI_ATLAS_SLOT_COUNT 5

typedef enum {
    WORLDGEN_UI_ASSET_LANDMASS_OCEAN_XY,
    WORLDGEN_UI_ASSET_RELIEF_PROFILE,
    WORLDGEN_UI_ASSET_CLIMATE_BIOME_ENVELOPE,
    WORLDGEN_UI_ASSET_RIVER_DENSITY_ATLAS,
    WORLDGEN_UI_ASSET_NATURAL_REGION_SCALE_ATLAS,
    WORLDGEN_UI_ASSET_COUNT
} WorldgenUiAssetId;

typedef struct {
    const char *path;
    int expected_width;
    int expected_height;
    int slot_count;
} WorldgenUiAssetInfo;

typedef struct {
    unsigned int attempts;
    unsigned int decodes;
    unsigned int draw_calls;
    unsigned int fallback_draws;
    unsigned int bitmap_allocations;
    unsigned int bitmap_releases;
    unsigned int surface_builds;
    unsigned int surface_hits;
    unsigned int surface_allocations;
    unsigned int surface_releases;
    int loaded;
    int width;
    int height;
} WorldgenUiAssetDiagnostics;

const WorldgenUiAssetInfo *worldgen_ui_assets_info(WorldgenUiAssetId asset);
int worldgen_ui_assets_preload_all(void);
int worldgen_ui_assets_is_loaded(WorldgenUiAssetId asset);
int worldgen_ui_assets_dimensions(WorldgenUiAssetId asset,
                                  int *width, int *height);
int worldgen_ui_assets_draw_fit(HDC hdc, WorldgenUiAssetId asset, RECT dst);
int worldgen_ui_assets_draw_slot_fit(HDC hdc, WorldgenUiAssetId asset,
                                     int slot, RECT dst);
int worldgen_ui_assets_get_diagnostics(
    WorldgenUiAssetId asset, WorldgenUiAssetDiagnostics *out);
void worldgen_ui_assets_get_totals(WorldgenUiAssetDiagnostics *out);

/* Release permits a new one-attempt cache lifetime; reset also clears counters. */
void worldgen_ui_assets_release(void);
void worldgen_ui_assets_reset_for_tests(void);

/* Validation-only deterministic missing-file path; reset always clears it. */
void worldgen_ui_assets_validation_force_missing(
    WorldgenUiAssetId asset, int force_missing);

#endif
