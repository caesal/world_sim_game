#include "render/render_ocean_static_mask.h"

#include "render/render_water_coverage.h"

#include <string.h>
#include <windows.h>

RenderOceanStaticMaskStats ocean_static_mask_debug_stats;

static unsigned int average_coverage(int x, int y, int lake) {
    unsigned int sum = 0;
    int dx, dy;
    for (dy = 0; dy < 2; dy++) {
        for (dx = 0; dx < 2; dx++) {
            sum += lake ? render_water_coverage_lake_alpha(x + dx, y + dy) :
                          render_water_coverage_ocean_alpha(x + dx, y + dy);
        }
    }
    return (sum + 2u) / 4u;
}

static unsigned int scaled_channel(unsigned int value, unsigned int alpha) {
    return (value * alpha + 127u) / 255u;
}

static uint32_t premultiply_inverse_ocean(uint32_t pixel,
                                          unsigned int ocean_alpha) {
    unsigned int alpha = 255u - ocean_alpha;
    unsigned int blue = scaled_channel(pixel & 255u, alpha);
    unsigned int green = scaled_channel((pixel >> 8) & 255u, alpha);
    unsigned int red = scaled_channel((pixel >> 16) & 255u, alpha);
    unsigned int source_alpha = (pixel >> 24) & 255u;
    unsigned int output_alpha = scaled_channel(source_alpha, alpha);
    return blue | (green << 8) | (red << 16) | (output_alpha << 24);
}

int render_ocean_static_mask_apply(uint32_t *pixels, int width, int height,
                                   const RenderSnapshot *snapshot) {
    int x, y;
    if (!pixels || width <= 0 || height <= 0 || !snapshot ||
        !snapshot->world_generated || !GdiFlush() ||
        !render_water_coverage_prepare(snapshot) ||
        render_water_coverage_width() != width * 2 ||
        render_water_coverage_height() != height * 2) {
        ocean_static_mask_debug_stats.failures++;
        return 0;
    }
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            unsigned int ocean = average_coverage(x * 2, y * 2, 0);
            unsigned int lake = average_coverage(x * 2, y * 2, 1);
            uint32_t *pixel = &pixels[y * width + x];
            if (lake > 0) {
                ocean = 0;
                ocean_static_mask_debug_stats.lake_opaque_pixels++;
            }
            if (ocean == 0) {
                ocean_static_mask_debug_stats.opaque_pixels++;
            } else {
                *pixel = premultiply_inverse_ocean(*pixel, ocean);
                if (ocean == 255)
                    ocean_static_mask_debug_stats.transparent_ocean_pixels++;
                else ocean_static_mask_debug_stats.partial_ocean_pixels++;
            }
        }
    }
    ocean_static_mask_debug_stats.applications++;
    ocean_static_mask_debug_stats.pixel_scans +=
        (uint64_t)width * (uint64_t)height;
    ocean_static_mask_debug_stats.coverage_samples +=
        (uint64_t)width * (uint64_t)height * 8u;
    ocean_static_mask_debug_stats.last_width = width;
    ocean_static_mask_debug_stats.last_height = height;
    return 1;
}

const RenderOceanStaticMaskStats *render_ocean_static_mask_stats(void) {
    return &ocean_static_mask_debug_stats;
}

void render_ocean_static_mask_reset_debug(void) {
    memset(&ocean_static_mask_debug_stats, 0,
           sizeof(ocean_static_mask_debug_stats));
}
