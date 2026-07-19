#include "render/render_ocean_coverage.h"

#include "render/render_common.h"
#include "render/render_water_coverage.h"

#include <string.h>

static RenderOceanCoverageStats stats;

static void refresh_stats(void) {
    const RenderWaterCoverageStats *coverage = render_water_coverage_stats();
    stats.field_rebuilds = (int)coverage->rebuilds;
    stats.field_tile_scans = coverage->source_scans;
    stats.field_raster_samples = coverage->raster_samples;
    stats.field_retained_bytes = coverage->retained_bytes;
    stats.ocean_tiles = coverage->ocean_tiles;
    stats.lake_tiles = coverage->lake_tiles;
    stats.scale = render_water_coverage_scale();
}

int render_ocean_coverage_prepare_field(const RenderSnapshot *snapshot) {
    if (!render_water_coverage_prepare(snapshot)) return 0;
    refresh_stats();
    return 1;
}

int render_ocean_coverage_pixel_is_ocean(const RenderSnapshot *snapshot,
                                         MapLayout layout, int px, int py) {
    int width, height, x, y;
    if (!snapshot || layout.draw_w <= 0 || layout.draw_h <= 0 ||
        px < layout.map_x || py < layout.map_y ||
        px >= layout.map_x + layout.draw_w ||
        py >= layout.map_y + layout.draw_h ||
        !render_ocean_coverage_prepare_field(snapshot)) return 0;
    width = render_water_coverage_width();
    height = render_water_coverage_height();
    x = (int)((((int64_t)(px - layout.map_x) * 2 + 1) * width) /
              ((int64_t)layout.draw_w * 2));
    y = (int)((((int64_t)(py - layout.map_y) * 2 + 1) * height) /
              ((int64_t)layout.draw_h * 2));
    return render_water_coverage_ocean_alpha(
               clamp(x, 0, width - 1), clamp(y, 0, height - 1)) >= 128;
}

const RenderOceanCoverageStats *render_ocean_coverage_stats(void) {
    refresh_stats();
    return &stats;
}

void render_ocean_coverage_invalidate(void) {
    render_water_coverage_invalidate();
    memset(&stats, 0, sizeof(stats));
}

void render_ocean_coverage_reset_debug(void) {
    uint64_t retained = render_water_coverage_stats()->retained_bytes;
    render_water_coverage_reset_debug();
    memset(&stats, 0, sizeof(stats));
    stats.field_retained_bytes = retained;
    refresh_stats();
}
