#include "render/render_water_surface_cache.h"

#include "render/render_common.h"
#include "render/render_ocean_assets.h"
#include "render/render_ocean_coverage.h"
#include "render/render_static_map_surface.h"
#include "render/render_water_coast_presentation.h"
#include "render/render_water_coverage.h"

#include <stdlib.h>
#include <string.h>

#define WATER_SURFACE_LAKE_DISPLAY_KEY 1

static MapLayerCache lake_surface;
static unsigned int source_key;
static int source_map_w;
static int source_map_h;
static int source_terrain_revision;
static int source_coast_revision;
static int source_hydrology_revision;
static int source_texture_ready;
RenderWaterSurfaceCacheStats water_surface_cache_debug_stats;
#define stats water_surface_cache_debug_stats

static unsigned int mix_key(unsigned int key, unsigned int value) {
    return (key * 1000003u) ^ value;
}

static unsigned int water_key(const RenderSnapshot *snapshot) {
    unsigned int key = 2166136261u;
    key = mix_key(key, (unsigned int)snapshot->map_w);
    key = mix_key(key, (unsigned int)snapshot->map_h);
    key = mix_key(key, (unsigned int)snapshot->terrain_revision);
    key = mix_key(key, (unsigned int)snapshot->coast_revision);
    key = mix_key(key, (unsigned int)snapshot->hydrology_revision);
    return mix_key(key, (unsigned int)ocean_assets_texture_loaded());
}

uint32_t render_water_surface_lake_tint_pixel(uint32_t pixel) {
    unsigned int red = (pixel >> 16) & 255u;
    unsigned int green = (pixel >> 8) & 255u;
    unsigned int blue = pixel & 255u;
    red = min(255u, (169u * red + 17613u + 128u) >> 8);
    green = min(255u, (147u * green + 24115u + 128u) >> 8);
    blue = min(255u, (122u * blue + 32525u + 128u) >> 8);
    return blue | (green << 8) | (red << 16) | 0xff000000u;
}

int render_water_surface_cache_pixel_is_lake(
    const RenderSnapshot *snapshot, MapLayout layout, int px, int py) {
    int width, height, x, y;
    if (!snapshot || layout.draw_w <= 0 || layout.draw_h <= 0 ||
        px < layout.map_x || py < layout.map_y ||
        px >= layout.map_x + layout.draw_w ||
        py >= layout.map_y + layout.draw_h ||
        !render_water_coverage_prepare(snapshot)) return 0;
    width = render_water_coverage_width();
    height = render_water_coverage_height();
    x = (int)((((int64_t)(px - layout.map_x) * 2 + 1) * width) /
              ((int64_t)layout.draw_w * 2));
    y = (int)((((int64_t)(py - layout.map_y) * 2 + 1) * height) /
              ((int64_t)layout.draw_h * 2));
    return render_water_coverage_lake_alpha(
               clamp(x, 0, width - 1), clamp(y, 0, height - 1)) >= 128;
}

static uint32_t premultiply_pixel(uint32_t pixel, unsigned int alpha) {
    unsigned int red = ((pixel >> 16) & 255u) * alpha;
    unsigned int green = ((pixel >> 8) & 255u) * alpha;
    unsigned int blue = (pixel & 255u) * alpha;
    red = (red + 127u) / 255u;
    green = (green + 127u) / 255u;
    blue = (blue + 127u) / 255u;
    return blue | (green << 8) | (red << 16) | (alpha << 24);
}

static int build_surface(HDC hdc, const RenderSnapshot *snapshot,
                         unsigned int key) {
    RenderWaterSurfaceCacheStats previous_stats = stats;
    MapLayerCache next_lake = {0};
    RenderWaterCoastPresentationMetrics coast = {0};
    unsigned char *categories = NULL;
    size_t tile_count;
    int width;
    int height;
    RECT full;
    int texture_ready;
    int x, y;
    if (!render_water_coverage_prepare(snapshot)) return 0;
    tile_count = (size_t)snapshot->map_w * (size_t)snapshot->map_h;
    width = render_water_coverage_width();
    height = render_water_coverage_height();
    full = (RECT){0, 0, width, height};
    if (!render_static_map_surface_ensure(hdc, &next_lake, width, height))
        goto failure;
    fill_rect(next_lake.dc, full, RGB(55, 135, 199));
    texture_ready = ocean_assets_draw_texture_tiled(
        next_lake.dc, full,
        ocean_assets_texture_tile_px() * render_water_coverage_scale());
    if (!texture_ready) goto failure;
    stats.texture_score = 900;
    if (!GdiFlush()) goto failure;
    stats.ocean_tiles = 0;
    stats.lake_tiles = 0;
    categories = (unsigned char *)malloc(tile_count);
    if (!categories || !render_water_coast_presentation_build(
            snapshot, categories, &coast)) goto failure;
    stats.ocean_tiles = (int)coast.semantic_ocean_tiles;
    stats.lake_tiles = (int)coast.semantic_lake_tiles;
    stats.coast_thin_components = coast.ocean_fringe.thin_components +
        coast.land_fringe.thin_components +
        coast.ocean_cleanup.thin_components;
    stats.coast_one_ended_components =
        coast.ocean_fringe.one_ended_components +
        coast.land_fringe.one_ended_components +
        coast.ocean_cleanup.one_ended_components;
    stats.coast_preserved_components =
        coast.ocean_fringe.preserved_components +
        coast.land_fringe.preserved_components +
        coast.ocean_cleanup.preserved_components;
    stats.coast_sparse_mesh_components =
        coast.ocean_fringe.sparse_mesh_components;
    stats.coast_sparse_mesh_tiles = coast.ocean_fringe.sparse_mesh_tiles;
    stats.coast_regularized_components =
        coast.ocean_fringe.regularized_components +
        coast.land_fringe.regularized_components +
        coast.ocean_cleanup.regularized_components;
    stats.coast_removed_ocean_tiles = coast.removed_ocean_tiles;
    stats.coast_filled_land_tiles = coast.filled_land_tiles;
    stats.coast_regularized_tiles = coast.removed_ocean_tiles +
        coast.filled_land_tiles;
    stats.coast_regularized_pixels = 0;
    stats.coast_removed_ocean_pixels = 0;
    stats.coast_filled_land_pixels = 0;
    stats.coast_protected_land_tiles = coast.protected_land_tiles +
        coast.protected_lake_neighbor_tiles;
    stats.coast_protected_marine_tiles = coast.protected_marine_tiles;
    stats.coast_protected_marine_components =
        coast.initial_protected_marine_components +
        coast.ocean_cleanup_protected_components;
    stats.coast_cleanup_tiles = coast.ocean_cleanup_tiles;
    stats.coast_presentation_hash = coast.presentation_hash;
    stats.coast_candidate_comparisons =
        coast.ocean_fringe.candidate_comparisons +
        coast.land_fringe.candidate_comparisons +
        coast.ocean_cleanup.candidate_comparisons;
    stats.coast_smoothing_transient_bytes =
        coast.transient_bytes + tile_count;
    stats.ocean_pixels = 0;
    stats.lake_pixels = 0;
    stats.texture_inset_radius = 0;
    stats.ocean_partial_coverage_pixels = 0;
    stats.lake_partial_coverage_pixels = 0;
    stats.water_row_spans = 0;
    for (y = 0; y < height; y++) {
        int in_span = 0;
        for (x = 0; x < width; x++) {
            unsigned int lake = render_water_coverage_lake_alpha(x, y);
            unsigned int ocean = render_water_coverage_ocean_alpha(x, y);
            int index = y * width + x;
            int tile_index = (y / render_water_coverage_scale()) *
                                 snapshot->map_w +
                             x / render_water_coverage_scale();
            const SnapshotTile *tile = &snapshot->tiles[tile_index];
            int semantic_ocean = tile->geography == GEO_OCEAN ||
                                 tile->geography == GEO_BAY;
            int presentation_ocean =
                categories[tile_index] == WATER_COAST_PRESENTATION_OCEAN;
            if (semantic_ocean && !presentation_ocean) {
                stats.coast_removed_ocean_pixels++;
                stats.coast_regularized_pixels++;
            } else if (!semantic_ocean &&
                       tile->geography != GEO_LAKE &&
                       presentation_ocean && ocean) {
                stats.coast_filled_land_pixels++;
                stats.coast_regularized_pixels++;
            }
            if (lake) {
                uint32_t tinted = render_water_surface_lake_tint_pixel(
                    next_lake.pixels[index]);
                stats.lake_pixels++;
                stats.lake_partial_coverage_pixels += lake < 255u;
                next_lake.pixels[index] = premultiply_pixel(tinted, lake);
            } else next_lake.pixels[index] = 0;
            if (ocean) {
                stats.ocean_pixels++;
                stats.ocean_partial_coverage_pixels += ocean < 255u;
            }
            if ((lake || ocean) && !in_span) {
                stats.water_row_spans++;
                in_span = 1;
            } else if (!lake && !ocean) {
                in_span = 0;
            }
        }
    }
    stats.mask_pixel_scans += (uint64_t)width * (uint64_t)height;
    key = water_key(snapshot);
    source_key = key;
    source_map_w = snapshot->map_w;
    source_map_h = snapshot->map_h;
    source_terrain_revision = snapshot->terrain_revision;
    source_coast_revision = snapshot->coast_revision;
    source_hydrology_revision = snapshot->hydrology_revision;
    source_texture_ready = ocean_assets_texture_loaded();
    render_static_map_surface_mark_valid(&next_lake, (int)source_key,
                                         WATER_SURFACE_LAKE_DISPLAY_KEY, 1);
    free(categories);
    render_static_map_surface_release(&lake_surface);
    lake_surface = next_lake;
    stats.rebuilds++;
    return 1;
failure:
    stats = previous_stats;
    free(categories);
    render_static_map_surface_release(&next_lake);
    return 0;
}

int render_water_surface_cache_ensure(HDC hdc,
                                      const RenderSnapshot *snapshot) {
    unsigned int key;
    int width, height;
    if (!hdc || !snapshot || !snapshot->world_generated ||
        snapshot->map_w <= 0 || snapshot->map_h <= 0) return 0;
    if (!ocean_assets_texture_ready()) return 0;
    key = water_key(snapshot);
    width = snapshot->map_w * render_water_coverage_scale();
    height = snapshot->map_h * render_water_coverage_scale();
    if (source_key == key && render_static_map_surface_matches(
            &lake_surface, width, height, (int)key,
            WATER_SURFACE_LAKE_DISPLAY_KEY)) {
        stats.cache_hits++;
        return 1;
    }
    return build_surface(hdc, snapshot, key);
}

int render_water_surface_cache_ready(const RenderSnapshot *snapshot) {
    unsigned int key;
    if (!snapshot || !snapshot->world_generated) return 0;
    key = water_key(snapshot);
    if (!source_key || source_key != key || source_map_w != snapshot->map_w ||
        source_map_h != snapshot->map_h ||
        source_terrain_revision != snapshot->terrain_revision ||
        source_coast_revision != snapshot->coast_revision ||
        source_hydrology_revision != snapshot->hydrology_revision ||
        source_texture_ready != ocean_assets_texture_loaded()) return 0;
    return render_static_map_surface_matches(
        &lake_surface, snapshot->map_w * render_water_coverage_scale(),
        snapshot->map_h * render_water_coverage_scale(), (int)key,
        WATER_SURFACE_LAKE_DISPLAY_KEY);
}

void render_water_surface_cache_present_lake(HDC hdc, RECT client,
                                             MapLayout layout,
                                             const RenderSnapshot *snapshot) {
    if (!hdc) return;
    if (!render_water_surface_cache_ready(snapshot)) return;
    render_static_map_surface_present_alpha(hdc, client, layout,
                                            &lake_surface, HALFTONE);
    stats.lake_presents++;
    stats.presents++;
}

void render_water_surface_cache_present_ocean(HDC hdc, RECT client,
                                              MapLayout layout,
                                              const RenderSnapshot *snapshot) {
    (void)hdc;
    (void)client;
    (void)layout;
    (void)snapshot;
}

void render_water_surface_cache_present(HDC hdc, RECT client,
                                        MapLayout layout,
                                        const RenderSnapshot *snapshot) {
    render_water_surface_cache_present_lake(hdc, client, layout, snapshot);
}

RenderLayerCacheMemory render_water_surface_cache_memory(void) {
    RenderLayerCacheMemory memory = {0};
    if (lake_surface.bitmap) {
        memory.bitmaps++;
        memory.bitmap_bytes += (uint64_t)lake_surface.width *
                               (uint64_t)lake_surface.height * 4u;
    }
    if (lake_surface.dc) memory.dcs++;
    return memory;
}

const RenderWaterSurfaceCacheStats *render_water_surface_cache_stats(void) {
    RenderLayerCacheMemory memory = render_water_surface_cache_memory();
    stats.persistent_bitmaps = memory.bitmaps;
    stats.persistent_dcs = memory.dcs;
    stats.persistent_bitmap_bytes = memory.bitmap_bytes;
    return &stats;
}

void render_water_surface_cache_invalidate(void) {
    render_static_map_surface_release(&lake_surface);
    render_ocean_coverage_invalidate();
    source_key = 0;
    source_map_w = source_map_h = 0;
    source_terrain_revision = source_coast_revision = 0;
    source_hydrology_revision = source_texture_ready = 0;
}

void render_water_surface_cache_reset_debug(void) {
    RenderLayerCacheMemory memory = render_water_surface_cache_memory();
    memset(&stats, 0, sizeof(stats));
    stats.interior_water_only = 1;
    stats.interior_deep_only = 1;
    stats.interior_min_clearance = 99;
    stats.persistent_bitmaps = memory.bitmaps;
    stats.persistent_dcs = memory.dcs;
    stats.persistent_bitmap_bytes = memory.bitmap_bytes;
}
