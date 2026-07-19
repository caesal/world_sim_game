#include "render/render_water_coverage.h"
#include "render/render_water_coverage_raster.h"
#include "render/render_water_coast_presentation.h"

#include "core/world_types.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define WATER_COVERAGE_SCALE 4
#define COVERAGE_SUBSAMPLES 4

typedef struct {
    unsigned char *ocean_alpha;
    unsigned char *lake_alpha;
    int map_w;
    int map_h;
    int width;
    int height;
    int terrain_revision;
    int coast_revision;
    int hydrology_revision;
    int valid;
} WaterCoverageCache;

static WaterCoverageCache cache;
static RenderWaterCoverageStats stats;

static int cache_matches(const RenderSnapshot *snapshot) {
    return cache.valid && cache.ocean_alpha && cache.lake_alpha &&
           cache.map_w == snapshot->map_w && cache.map_h == snapshot->map_h &&
           cache.terrain_revision == snapshot->terrain_revision &&
           cache.coast_revision == snapshot->coast_revision &&
           cache.hydrology_revision == snapshot->hydrology_revision;
}

static void release_cache(void) {
    free(cache.ocean_alpha);
    free(cache.lake_alpha);
    memset(&cache, 0, sizeof(cache));
    stats.retained_bytes = 0;
}

int render_water_coverage_prepare(const RenderSnapshot *snapshot) {
    RenderWaterCoverageRasterMetrics raster_metrics;
    RenderWaterCoastPresentationMetrics coast_metrics;
    unsigned char *source = NULL;
    unsigned char *ocean_alpha = NULL;
    unsigned char *lake_alpha = NULL;
    unsigned char *category_pixels = NULL;
    size_t source_count, raster_count;
    int width, height;
    if (!snapshot || !snapshot->world_generated || snapshot->map_w <= 0 ||
        snapshot->map_h <= 0 || snapshot->map_w > MAX_MAP_W ||
        snapshot->map_h > MAX_MAP_H ||
        snapshot->map_w > INT_MAX / WATER_COVERAGE_SCALE ||
        snapshot->map_h > INT_MAX / WATER_COVERAGE_SCALE) return 0;
    if (cache_matches(snapshot)) return 1;
    if ((size_t)snapshot->map_w > SIZE_MAX / (size_t)snapshot->map_h) return 0;
    source_count = (size_t)snapshot->map_w * (size_t)snapshot->map_h;
    if (source_count > SIZE_MAX /
                       (WATER_COVERAGE_SCALE * WATER_COVERAGE_SCALE)) return 0;
    raster_count = source_count * WATER_COVERAGE_SCALE * WATER_COVERAGE_SCALE;
    width = snapshot->map_w * WATER_COVERAGE_SCALE;
    height = snapshot->map_h * WATER_COVERAGE_SCALE;
    source = (unsigned char *)malloc(source_count);
    ocean_alpha = (unsigned char *)malloc(raster_count);
    lake_alpha = (unsigned char *)malloc(raster_count);
    category_pixels = (unsigned char *)malloc(raster_count);
    if (!source || !ocean_alpha || !lake_alpha || !category_pixels) {
        free(source);
        free(ocean_alpha);
        free(lake_alpha);
        free(category_pixels);
        return 0;
    }
    if (!render_water_coast_presentation_build(
            snapshot, source, &coast_metrics)) {
        free(source);
        free(ocean_alpha);
        free(lake_alpha);
        free(category_pixels);
        return 0;
    }
    if (!render_water_coverage_rasterize(
            source, snapshot->map_w, snapshot->map_h,
            WATER_COVERAGE_SCALE, COVERAGE_SUBSAMPLES,
            ocean_alpha, lake_alpha, category_pixels, &raster_metrics)) {
        free(category_pixels);
        free(source);
        free(ocean_alpha);
        free(lake_alpha);
        return 0;
    }
    free(category_pixels);
    free(source);
    release_cache();
    cache.ocean_alpha = ocean_alpha;
    cache.lake_alpha = lake_alpha;
    cache.map_w = snapshot->map_w;
    cache.map_h = snapshot->map_h;
    cache.width = width;
    cache.height = height;
    cache.terrain_revision = snapshot->terrain_revision;
    cache.coast_revision = snapshot->coast_revision;
    cache.hydrology_revision = snapshot->hydrology_revision;
    cache.valid = 1;
    stats.rebuilds++;
    stats.source_scans += source_count;
    stats.raster_samples += raster_metrics.raster_samples;
    stats.retained_bytes = (uint64_t)raster_count * 2u;
    stats.lake_diagonal_cells = raster_metrics.lake_diagonal_cells;
    stats.lake_land_pixels_rejected =
        raster_metrics.lake_land_pixels_rejected;
    stats.ocean_land_pixels_rejected =
        raster_metrics.ocean_land_pixels_rejected;
    stats.ocean_overlap_pixels_removed =
        raster_metrics.ocean_overlap_pixels_removed;
    stats.coast_removed_ocean_tiles = coast_metrics.removed_ocean_tiles;
    stats.coast_filled_land_tiles = coast_metrics.filled_land_tiles;
    stats.coast_protected_land_tiles = coast_metrics.protected_land_tiles;
    stats.coast_protected_lake_neighbor_tiles =
        coast_metrics.protected_lake_neighbor_tiles;
    stats.coast_protected_marine_tiles =
        coast_metrics.protected_marine_tiles;
    stats.coast_protected_marine_components =
        coast_metrics.initial_protected_marine_components +
        coast_metrics.ocean_cleanup_protected_components;
    stats.coast_cleanup_tiles = coast_metrics.ocean_cleanup_tiles;
    stats.coast_presentation_hash = coast_metrics.presentation_hash;
    stats.coast_presentation_transient_bytes = coast_metrics.transient_bytes;
    stats.ocean_tiles = (int)coast_metrics.semantic_ocean_tiles;
    stats.lake_tiles = (int)coast_metrics.semantic_lake_tiles;
    return 1;
}

static unsigned char alpha_at(const unsigned char *alpha, int x, int y) {
    if (!cache.valid || !alpha || x < 0 || y < 0 ||
        x >= cache.width || y >= cache.height) return 0;
    return alpha[y * cache.width + x];
}

unsigned char render_water_coverage_ocean_alpha(int x, int y) {
    return alpha_at(cache.ocean_alpha, x, y);
}

unsigned char render_water_coverage_lake_alpha(int x, int y) {
    return alpha_at(cache.lake_alpha, x, y);
}

unsigned char render_water_coverage_total_alpha(int x, int y) {
    unsigned int ocean = render_water_coverage_ocean_alpha(x, y);
    unsigned int lake = render_water_coverage_lake_alpha(x, y);
    return (unsigned char)min(255u, ocean + lake);
}

int render_water_coverage_width(void) { return cache.valid ? cache.width : 0; }
int render_water_coverage_height(void) { return cache.valid ? cache.height : 0; }
int render_water_coverage_scale(void) { return WATER_COVERAGE_SCALE; }

const RenderWaterCoverageStats *render_water_coverage_stats(void) {
    return &stats;
}

void render_water_coverage_invalidate(void) { release_cache(); }

void render_water_coverage_reset_debug(void) {
    uint64_t retained_bytes = stats.retained_bytes;
    uint64_t lake_diagonal_cells = stats.lake_diagonal_cells;
    uint64_t lake_land_pixels_rejected = stats.lake_land_pixels_rejected;
    uint64_t ocean_land_pixels_rejected = stats.ocean_land_pixels_rejected;
    uint64_t ocean_overlap_pixels_removed =
        stats.ocean_overlap_pixels_removed;
    uint64_t coast_removed_ocean_tiles = stats.coast_removed_ocean_tiles;
    uint64_t coast_filled_land_tiles = stats.coast_filled_land_tiles;
    uint64_t coast_protected_land_tiles = stats.coast_protected_land_tiles;
    uint64_t coast_protected_lake_neighbor_tiles =
        stats.coast_protected_lake_neighbor_tiles;
    uint64_t coast_protected_marine_tiles =
        stats.coast_protected_marine_tiles;
    uint64_t coast_protected_marine_components =
        stats.coast_protected_marine_components;
    uint64_t coast_cleanup_tiles = stats.coast_cleanup_tiles;
    uint64_t coast_presentation_hash = stats.coast_presentation_hash;
    uint64_t coast_presentation_transient_bytes =
        stats.coast_presentation_transient_bytes;
    int ocean_tiles = stats.ocean_tiles;
    int lake_tiles = stats.lake_tiles;
    memset(&stats, 0, sizeof(stats));
    stats.retained_bytes = retained_bytes;
    stats.lake_diagonal_cells = lake_diagonal_cells;
    stats.lake_land_pixels_rejected = lake_land_pixels_rejected;
    stats.ocean_land_pixels_rejected = ocean_land_pixels_rejected;
    stats.ocean_overlap_pixels_removed = ocean_overlap_pixels_removed;
    stats.coast_removed_ocean_tiles = coast_removed_ocean_tiles;
    stats.coast_filled_land_tiles = coast_filled_land_tiles;
    stats.coast_protected_land_tiles = coast_protected_land_tiles;
    stats.coast_protected_lake_neighbor_tiles =
        coast_protected_lake_neighbor_tiles;
    stats.coast_protected_marine_tiles = coast_protected_marine_tiles;
    stats.coast_protected_marine_components =
        coast_protected_marine_components;
    stats.coast_cleanup_tiles = coast_cleanup_tiles;
    stats.coast_presentation_hash = coast_presentation_hash;
    stats.coast_presentation_transient_bytes =
        coast_presentation_transient_bytes;
    stats.ocean_tiles = ocean_tiles;
    stats.lake_tiles = lake_tiles;
}
