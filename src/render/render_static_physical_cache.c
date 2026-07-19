#include "render/render_static_physical_cache.h"

#include "render/map_display_policy.h"
#include "render/map_shore_color_cache.h"
#include "render/coast_geometry.h"
#include "render/render_allocation_diagnostics.h"
#include "render/render_common.h"
#include "render/render_water_coast_presentation.h"

#include <stdlib.h>
#include <string.h>

#define PHYSICAL_CACHE_SCALE 2

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    unsigned int *pixels;
    int width;
    int height;
    unsigned int key;
    int valid;
} PhysicalSurface;

static PhysicalSurface base_surfaces[MAP_PHYSICAL_BASE_COUNT];
static unsigned int cached_coast_key;
static int cached_coast_valid;
static RenderStaticPhysicalCacheStats stats;

static unsigned int mix_key(unsigned int key, unsigned int value) {
    return (key * 1000003u) ^ value;
}

static unsigned int dimensions_key(const RenderSnapshot *snapshot) {
    return mix_key((unsigned int)snapshot->map_w, (unsigned int)snapshot->map_h);
}

static void release_surface(PhysicalSurface *surface) {
    if (surface->dc && surface->old_bitmap &&
        (HGDIOBJ)surface->old_bitmap != HGDI_ERROR)
        SelectObject(surface->dc, surface->old_bitmap);
    if (surface->bitmap) DeleteObject(surface->bitmap);
    if (surface->dc) DeleteDC(surface->dc);
    memset(surface, 0, sizeof(*surface));
}

static void refresh_memory_stats(void) {
    int i;
    stats.persistent_bitmaps = 0;
    stats.persistent_dcs = 0;
    stats.persistent_bitmap_bytes = 0;
    for (i = 0; i < MAP_PHYSICAL_BASE_COUNT; i++) {
        const PhysicalSurface *surface = &base_surfaces[i];
        if (surface->bitmap) {
            stats.persistent_bitmaps++;
            stats.persistent_bitmap_bytes +=
                (uint64_t)surface->width * (uint64_t)surface->height * 4u;
        }
        if (surface->dc) stats.persistent_dcs++;
    }
}

static int ensure_surface(HDC hdc, PhysicalSurface *surface,
                          const RenderSnapshot *snapshot) {
    BITMAPINFO info;
    int width = snapshot->map_w * PHYSICAL_CACHE_SCALE;
    int height = snapshot->map_h * PHYSICAL_CACHE_SCALE;
    if (surface->dc && surface->bitmap && surface->pixels &&
        surface->width == width && surface->height == height) return 1;
    release_surface(surface);
    render_allocation_note_attempt(RENDER_ALLOCATION_PHYSICAL, width, height);
    if (render_allocation_inject_failure(RENDER_ALLOCATION_PHYSICAL,
                                         width, height)) {
        refresh_memory_stats();
        return 0;
    }
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    surface->dc = CreateCompatibleDC(hdc);
    surface->bitmap = CreateDIBSection(hdc, &info, DIB_RGB_COLORS,
                                       (void **)&surface->pixels, NULL, 0);
    if (!surface->dc || !surface->bitmap || !surface->pixels) {
        render_allocation_note_failure(RENDER_ALLOCATION_PHYSICAL, width, height);
        release_surface(surface);
        refresh_memory_stats();
        return 0;
    }
    surface->old_bitmap = SelectObject(surface->dc, surface->bitmap);
    if (!surface->old_bitmap ||
        (HGDIOBJ)surface->old_bitmap == HGDI_ERROR) {
        surface->old_bitmap = NULL;
        render_allocation_note_failure(RENDER_ALLOCATION_PHYSICAL,
                                       width, height);
        release_surface(surface);
        refresh_memory_stats();
        return 0;
    }
    surface->width = width;
    surface->height = height;
    refresh_memory_stats();
    return 1;
}

static unsigned int packed_color(COLORREF color) {
    return (unsigned int)GetBValue(color) |
           ((unsigned int)GetGValue(color) << 8) |
           ((unsigned int)GetRValue(color) << 16) | 0xff000000u;
}

static void fill_tile(PhysicalSurface *surface, int x, int y, unsigned int value) {
    int px = x * PHYSICAL_CACHE_SCALE;
    int py = y * PHYSICAL_CACHE_SCALE;
    int dy;
    for (dy = 0; dy < PHYSICAL_CACHE_SCALE; dy++) {
        unsigned int *row = &surface->pixels[(py + dy) * surface->width + px];
        int dx;
        for (dx = 0; dx < PHYSICAL_CACHE_SCALE; dx++) row[dx] = value;
    }
}

static unsigned int base_key(const RenderSnapshot *snapshot, int family) {
    unsigned int key = dimensions_key(snapshot);
    key = mix_key(key, (unsigned int)snapshot->terrain_revision);
    key = mix_key(key, (unsigned int)snapshot->coast_revision);
    key = mix_key(key, (unsigned int)snapshot->hydrology_revision);
    key = mix_key(key, (unsigned int)family);
    return key;
}

static unsigned char *build_coast_presentation_categories(
    const RenderSnapshot *snapshot) {
    RenderWaterCoastPresentationMetrics metrics;
    size_t count = (size_t)snapshot->map_w * (size_t)snapshot->map_h;
    unsigned char *categories = (unsigned char *)malloc(count);
    stats.coast_regularized_tiles = 0;
    stats.coast_removed_ocean_tiles = 0;
    stats.coast_filled_land_tiles = 0;
    stats.coast_protected_land_tiles = 0;
    stats.coast_protected_marine_tiles = 0;
    stats.coast_protected_marine_components = 0;
    stats.coast_cleanup_tiles = 0;
    stats.coast_presentation_hash = 0;
    stats.coast_regularization_transient_bytes = 0;
    if (!categories || !render_water_coast_presentation_build(
            snapshot, categories, &metrics)) goto failure;
    stats.coast_removed_ocean_tiles = metrics.removed_ocean_tiles;
    stats.coast_filled_land_tiles = metrics.filled_land_tiles;
    stats.coast_protected_land_tiles = metrics.protected_land_tiles +
        metrics.protected_lake_neighbor_tiles;
    stats.coast_protected_marine_tiles =
        metrics.protected_marine_tiles;
    stats.coast_protected_marine_components =
        metrics.initial_protected_marine_components +
        metrics.ocean_cleanup_protected_components;
    stats.coast_cleanup_tiles = metrics.ocean_cleanup_tiles;
    stats.coast_regularized_tiles = metrics.removed_ocean_tiles +
        metrics.filled_land_tiles;
    stats.coast_presentation_hash = metrics.presentation_hash;
    stats.coast_regularization_transient_bytes =
        metrics.transient_bytes + count;
    return categories;
failure:
    free(categories);
    return NULL;
}

static int ensure_base(HDC hdc, const RenderSnapshot *snapshot, int family,
                       const unsigned char *coast_categories) {
    PhysicalSurface *surface;
    unsigned int key;
    int i;
    family = clamp(family, 0, MAP_PHYSICAL_BASE_COUNT - 1);
    surface = &base_surfaces[family];
    key = base_key(snapshot, family);
    if (surface->valid && surface->key == key) return 1;
    if (!map_shore_color_cache_prepare(snapshot)) return 0;
    if (!ensure_surface(hdc, surface, snapshot)) return 0;
    for (i = 0; i < snapshot->map_w * snapshot->map_h; i++) {
        int x = i % snapshot->map_w;
        int y = i / snapshot->map_w;
        const SnapshotTile *tile = &snapshot->tiles[i];
        COLORREF color = map_display_policy_snapshot_smoothed_physical_color(
            snapshot, tile, (MapPhysicalBaseFamily)family);
        if ((tile->geography == GEO_OCEAN || tile->geography == GEO_BAY) &&
            coast_categories &&
            coast_categories[i] == WATER_COAST_PRESENTATION_LAND)
            color = map_display_policy_snapshot_nearby_land_color(
                snapshot, tile, (MapPhysicalBaseFamily)family);
        fill_tile(surface, x, y, packed_color(color));
    }
    surface->key = key;
    surface->valid = 1;
    stats.base_rebuilds[family]++;
    stats.base_tile_scans[family] += (uint64_t)snapshot->map_w * snapshot->map_h;
    stats.ready_base_mask |= 1u << family;
    return 1;
}

static unsigned int coast_key(const RenderSnapshot *snapshot) {
    unsigned int key = dimensions_key(snapshot);
    key = mix_key(key, (unsigned int)snapshot->terrain_revision);
    return mix_key(key, (unsigned int)snapshot->coast_revision);
}

static int ensure_coast(HDC hdc, const RenderSnapshot *snapshot) {
    unsigned int key = coast_key(snapshot);
    (void)hdc;
    if (cached_coast_valid && cached_coast_key == key) return 1;
    cached_coast_key = key;
    cached_coast_valid = 1;
    stats.coast_rebuilds++;
    stats.coast_ready = 1;
    return 1;
}

int render_static_physical_cache_ensure_selected(HDC hdc, const RenderSnapshot *snapshot,
                                                 int mode, int river_lod, int wind_lod) {
    unsigned char *coast_categories = NULL;
    int family;
    int needs_base;
    int ok;
    if (!hdc || !snapshot || !snapshot->world_generated) return 0;
    (void)river_lod;
    (void)wind_lod;
    family = map_display_policy_physical_family(mode);
    needs_base = !base_surfaces[family].valid ||
        base_surfaces[family].key != base_key(snapshot, family);
    if (needs_base)
        coast_categories = build_coast_presentation_categories(snapshot);
    ok = ensure_base(hdc, snapshot, family, coast_categories) &&
         ensure_coast(hdc, snapshot);
    free(coast_categories);
    return ok;
}

int render_static_physical_cache_prewarm(HDC hdc, const RenderSnapshot *snapshot) {
    DWORD start = GetTickCount();
    unsigned char *coast_categories = NULL;
    int family;
    int ok = hdc && snapshot && snapshot->world_generated;
    stats.prewarm_attempts++;
    if (ok) coast_categories = build_coast_presentation_categories(snapshot);
    for (family = 0; ok && family < MAP_PHYSICAL_BASE_COUNT; family++)
        ok = ensure_base(hdc, snapshot, family, coast_categories);
    free(coast_categories);
    if (ok) ok = ensure_coast(hdc, snapshot);
    stats.prewarm_last_ms = (int)(GetTickCount() - start);
    if (ok) stats.prewarm_completions++;
    refresh_memory_stats();
    return ok;
}

int render_static_physical_cache_compose_base_coast(HDC destination, int mode) {
    int family = map_display_policy_physical_family(mode);
    const PhysicalSurface *base = &base_surfaces[family];
    if (!destination || !base->valid || !cached_coast_valid) return 0;
    BitBlt(destination, 0, 0, base->width, base->height, base->dc, 0, 0, SRCCOPY);
    return 1;
}

int render_static_physical_cache_width(void) {
    int family;
    for (family = 0; family < MAP_PHYSICAL_BASE_COUNT; family++) {
        if (base_surfaces[family].valid) return base_surfaces[family].width;
    }
    return 0;
}

int render_static_physical_cache_height(void) {
    int family;
    for (family = 0; family < MAP_PHYSICAL_BASE_COUNT; family++) {
        if (base_surfaces[family].valid) return base_surfaces[family].height;
    }
    return 0;
}

int render_static_physical_cache_selected_ready(const RenderSnapshot *snapshot,
                                                int mode, int river_lod, int wind_lod) {
    int family = map_display_policy_physical_family(mode);
    (void)river_lod;
    (void)wind_lod;
    if (!snapshot || !snapshot->world_generated) return 0;
    return base_surfaces[family].valid &&
           base_surfaces[family].key == base_key(snapshot, family) &&
           cached_coast_valid && cached_coast_key == coast_key(snapshot);
}

const RenderStaticPhysicalCacheStats *render_static_physical_cache_stats(void) {
    refresh_memory_stats();
    return &stats;
}

void render_static_physical_cache_reset_debug_counters(void) {
    int bitmaps = stats.persistent_bitmaps;
    int dcs = stats.persistent_dcs;
    uint64_t bytes = stats.persistent_bitmap_bytes;
    uint64_t regularized_tiles = stats.coast_regularized_tiles;
    uint64_t removed_ocean_tiles = stats.coast_removed_ocean_tiles;
    uint64_t filled_land_tiles = stats.coast_filled_land_tiles;
    uint64_t protected_land_tiles = stats.coast_protected_land_tiles;
    uint64_t protected_marine_tiles = stats.coast_protected_marine_tiles;
    uint64_t protected_marine_components =
        stats.coast_protected_marine_components;
    uint64_t cleanup_tiles = stats.coast_cleanup_tiles;
    uint64_t presentation_hash = stats.coast_presentation_hash;
    uint64_t regularization_bytes =
        stats.coast_regularization_transient_bytes;
    unsigned int base_mask = stats.ready_base_mask;
    int coast_ready = stats.coast_ready;
    memset(&stats, 0, sizeof(stats));
    stats.persistent_bitmaps = bitmaps;
    stats.persistent_dcs = dcs;
    stats.persistent_bitmap_bytes = bytes;
    stats.coast_regularized_tiles = regularized_tiles;
    stats.coast_removed_ocean_tiles = removed_ocean_tiles;
    stats.coast_filled_land_tiles = filled_land_tiles;
    stats.coast_protected_land_tiles = protected_land_tiles;
    stats.coast_protected_marine_tiles = protected_marine_tiles;
    stats.coast_protected_marine_components = protected_marine_components;
    stats.coast_cleanup_tiles = cleanup_tiles;
    stats.coast_presentation_hash = presentation_hash;
    stats.coast_regularization_transient_bytes = regularization_bytes;
    stats.ready_base_mask = base_mask;
    stats.coast_ready = coast_ready;
}

void render_static_physical_cache_invalidate(void) {
    int i;
    for (i = 0; i < MAP_PHYSICAL_BASE_COUNT; i++) release_surface(&base_surfaces[i]);
    cached_coast_key = 0;
    cached_coast_valid = 0;
    map_shore_color_cache_invalidate();
    coast_geometry_invalidate();
    memset(&stats, 0, sizeof(stats));
}
