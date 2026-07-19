#include "game/game_presentation_water_edge_probe.h"

#include "render/map_display_policy.h"
#include "render/render_static_physical_cache.h"
#include "render/render_water_coast_presentation.h"
#include "render/render_water_coverage.h"

#include <stdint.h>
#include <stdlib.h>

enum { PHYSICAL_SCALE = 2 };

static uint32_t packed_color(COLORREF color) {
    return ((uint32_t)GetRValue(color) << 16) |
           ((uint32_t)GetGValue(color) << 8) |
           (uint32_t)GetBValue(color);
}

static int semantic_category(int geography) {
    if (geography == GEO_LAKE) return WATER_COAST_PRESENTATION_LAKE;
    if (geography == GEO_OCEAN || geography == GEO_BAY)
        return WATER_COAST_PRESENTATION_OCEAN;
    return WATER_COAST_PRESENTATION_LAND;
}

int game_presentation_water_edge_base_contract(
    FILE *summary, const StaticPhysicalProbeCanvas *base,
    const RenderSnapshot *snapshot) {
    const RenderStaticPhysicalCacheStats *stats;
    RenderWaterCoastPresentationMetrics coast;
    size_t tile_count;
    unsigned char *categories = NULL;
    int category_failures = 0;
    int removed_ocean = 0;
    int center_failures = 0;
    int base_mismatches = 0;
    int land_centers = 0;
    int ocean_centers = 0;
    int lake_centers = 0;
    int x, y, dx, dy;
    if (!summary || !base || !snapshot ||
        !render_water_coverage_prepare(snapshot)) return 0;
    tile_count = (size_t)snapshot->map_w * (size_t)snapshot->map_h;
    categories = (unsigned char *)malloc(tile_count);
    if (!categories || !render_water_coast_presentation_build(
            snapshot, categories, &coast)) goto cleanup;
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            int index = y * snapshot->map_w + x;
            const SnapshotTile *tile = &snapshot->tiles[index];
            int semantic = semantic_category(tile->geography);
            int category = categories[index];
            int scale = render_water_coverage_scale();
            COLORREF expected_color =
                map_display_policy_snapshot_smoothed_physical_color(
                    snapshot, tile, MAP_PHYSICAL_BASE_GEOGRAPHY);
            uint32_t expected;
            category_failures += category != semantic;
            removed_ocean += semantic == WATER_COAST_PRESENTATION_OCEAN &&
                             category == WATER_COAST_PRESENTATION_LAND;
            expected = packed_color(expected_color);
            if (category == WATER_COAST_PRESENTATION_LAND)
                land_centers++;
            else if (category == WATER_COAST_PRESENTATION_OCEAN)
                ocean_centers++;
            else
                lake_centers++;
            for (dy = scale / 2 - 1; dy <= scale / 2; dy++) {
                for (dx = scale / 2 - 1; dx <= scale / 2; dx++) {
                    unsigned int ocean = render_water_coverage_ocean_alpha(
                        x * scale + dx, y * scale + dy);
                    unsigned int lake = render_water_coverage_lake_alpha(
                        x * scale + dx, y * scale + dy);
                    if (category == WATER_COAST_PRESENTATION_LAND)
                        center_failures += ocean != 0u || lake != 0u;
                    else if (category == WATER_COAST_PRESENTATION_OCEAN)
                        center_failures += ocean != 255u || lake != 0u;
                    else
                        center_failures += lake != 255u || ocean != 0u;
                }
            }
            for (dy = 0; dy < PHYSICAL_SCALE; dy++) {
                for (dx = 0; dx < PHYSICAL_SCALE; dx++) {
                    uint32_t actual = base->pixels[
                        (y * PHYSICAL_SCALE + dy) * base->width +
                        x * PHYSICAL_SCALE + dx] & UINT32_C(0x00ffffff);
                    base_mismatches += actual != expected;
                }
            }
        }
    }
    stats = render_static_physical_cache_stats();
    fprintf(summary,
            "case=static_semantic_physical_base ok=%d "
            "category_failures=%d removed_ocean=%d/%llu "
            "centers=%d land=%d ocean=%d lake=%d "
            "base_mismatches=%d removed=%llu filled=%llu cleanup=%llu "
            "cache_regularized=%llu transient=%llu hash=%llu\n",
            category_failures == 0 && center_failures == 0 &&
                base_mismatches == 0 && coast.removed_ocean_tiles == 0 &&
                removed_ocean == 0 &&
                coast.filled_land_tiles == 0 &&
                coast.ocean_cleanup_tiles == 0 &&
                stats->coast_regularized_tiles == 0,
            category_failures, removed_ocean,
            (unsigned long long)tile_count,
            center_failures, land_centers, ocean_centers, lake_centers,
            base_mismatches,
            (unsigned long long)coast.removed_ocean_tiles,
            (unsigned long long)coast.filled_land_tiles,
            (unsigned long long)coast.ocean_cleanup_tiles,
            (unsigned long long)stats->coast_regularized_tiles,
            (unsigned long long)coast.transient_bytes,
            (unsigned long long)coast.presentation_hash);
    free(categories);
    return category_failures == 0 && center_failures == 0 &&
           base_mismatches == 0 && coast.removed_ocean_tiles == 0 &&
           removed_ocean == 0 &&
           coast.filled_land_tiles == 0 &&
           coast.ocean_cleanup_tiles == 0 &&
           stats->coast_regularized_tiles == 0;
cleanup:
    free(categories);
    return 0;
}
