#include "render/render_static_physical_overlay_cache.h"

#include "render/render_allocation_diagnostics.h"
#include "render/river_geometry.h"
#include "render/river_render.h"
#include "render/wind_render.h"

#include <string.h>

typedef enum {
    OVERLAY_KIND_RIVER,
    OVERLAY_KIND_WIND
} OverlayKind;

typedef struct {
    int map_w;
    int map_h;
    int terrain_revision;
    int hydrology_revision;
    int overlay_revision;
    int field_revision;
    int lod;
    int scale;
    unsigned int style_key;
} OverlayKey;

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    unsigned int *pixels;
    int width;
    int height;
    int valid;
    int item_count;
    int target_count;
    int connected_count;
    OverlayKey key;
} OverlaySurface;

static OverlaySurface river_entries[RENDER_STATIC_OVERLAY_RIVER_LOD_COUNT];
static OverlaySurface wind_entries[RENDER_STATIC_OVERLAY_WIND_LOD_COUNT];
static RenderStaticPhysicalOverlayCacheStats stats;

static int keys_equal(const OverlayKey *a, const OverlayKey *b) {
    return memcmp(a, b, sizeof(*a)) == 0;
}

static unsigned int mix_key(unsigned int key, unsigned int value) {
    return key * 1000003u ^ value;
}

static unsigned int wind_style_key(void) {
    unsigned int key = (unsigned int)wind_render_style_color();
    key = mix_key(key, (unsigned int)wind_render_halo_color());
    key = mix_key(key, (unsigned int)wind_render_style_alpha());
    key = mix_key(key, (unsigned int)wind_render_style_thickness());
    key = mix_key(key, (unsigned int)wind_render_halo_thickness());
    return mix_key(key, (unsigned int)wind_render_style_head_percent());
}

static int kind_lod_count(OverlayKind kind) {
    return kind == OVERLAY_KIND_RIVER ?
        RENDER_STATIC_OVERLAY_RIVER_LOD_COUNT :
        RENDER_STATIC_OVERLAY_WIND_LOD_COUNT;
}

static int clamp_lod(OverlayKind kind, int lod) {
    return clamp(lod, 0, kind_lod_count(kind) - 1);
}

static int map_space_scale(OverlayKind kind, int lod) {
    lod = clamp_lod(kind, lod);
    return lod >= 2 ? 2 : 1;
}

static OverlayKey make_key(OverlayKind kind, const RenderSnapshot *snapshot,
                           int lod) {
    OverlayKey key;
    memset(&key, 0, sizeof(key));
    key.map_w = snapshot ? snapshot->map_w : 0;
    key.map_h = snapshot ? snapshot->map_h : 0;
    key.terrain_revision = snapshot ? snapshot->terrain_revision : 0;
    key.lod = clamp_lod(kind, lod);
    key.scale = map_space_scale(kind, key.lod);
    if (kind == OVERLAY_KIND_RIVER) {
        key.hydrology_revision = snapshot ? snapshot->hydrology_revision : 0;
        key.overlay_revision = snapshot ? snapshot->river_revision : 0;
        key.field_revision = snapshot ? snapshot->rivers.revision : 0;
    } else {
        key.overlay_revision = snapshot ? snapshot->wind_revision : 0;
        key.field_revision = snapshot ? snapshot->wind.revision : 0;
        key.style_key = wind_style_key();
    }
    return key;
}

static OverlaySurface *surface_for(OverlayKind kind, int lod) {
    lod = clamp_lod(kind, lod);
    return kind == OVERLAY_KIND_RIVER ?
        &river_entries[lod] : &wind_entries[lod];
}

static OverlaySurface *find_exact(OverlayKind kind, const OverlayKey *key) {
    OverlaySurface *surface = surface_for(kind, key->lod);
    return surface->valid && keys_equal(&surface->key, key) ? surface : NULL;
}

static void release_surface(OverlaySurface *surface) {
    if (surface->dc && surface->old_bitmap &&
        (HGDIOBJ)surface->old_bitmap != HGDI_ERROR)
        SelectObject(surface->dc, surface->old_bitmap);
    if (surface->bitmap) DeleteObject(surface->bitmap);
    if (surface->dc) DeleteDC(surface->dc);
    memset(surface, 0, sizeof(*surface));
}

static void refresh_memory_stats(void) {
    int kind;
    stats.persistent_bitmaps = 0;
    stats.persistent_dcs = 0;
    stats.persistent_bitmap_bytes = 0;
    stats.wind_anchor_bytes = wind_render_prepared_anchor_bytes();
    stats.wind_sprite_bytes = wind_render_sprite_atlas_bytes();
    stats.ready_river_entries = 0;
    stats.ready_wind_entries = 0;
    stats.ready_river_mask = 0;
    stats.ready_wind_mask = 0;
    for (kind = OVERLAY_KIND_RIVER; kind <= OVERLAY_KIND_WIND; kind++) {
        int count = kind_lod_count((OverlayKind)kind);
        int i;
        for (i = 0; i < count; i++) {
            const OverlaySurface *surface = surface_for((OverlayKind)kind, i);
            if (surface->bitmap) {
                stats.persistent_bitmaps++;
                stats.persistent_bitmap_bytes +=
                    (uint64_t)surface->width * (uint64_t)surface->height * 4u;
            }
            if (surface->dc) stats.persistent_dcs++;
            if (!surface->valid) continue;
            if (kind == OVERLAY_KIND_RIVER) {
                stats.ready_river_entries++;
                stats.ready_river_mask |= 1u << i;
            } else {
                stats.ready_wind_entries++;
                stats.ready_wind_mask |= 1u << i;
            }
        }
    }
}

static int ensure_surface(HDC hdc, OverlaySurface *surface,
                          int width, int height, OverlayKind kind) {
    BITMAPINFO info;
    if (width <= 0 || height <= 0) return 0;
    if (surface->dc && surface->bitmap && surface->pixels &&
        surface->width == width && surface->height == height) return 1;
    release_surface(surface);
    render_allocation_note_attempt(RENDER_ALLOCATION_PHYSICAL_OVERLAY, width, height);
    if (render_allocation_inject_failure(RENDER_ALLOCATION_PHYSICAL_OVERLAY,
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
        render_allocation_note_failure(RENDER_ALLOCATION_PHYSICAL_OVERLAY, width, height);
        release_surface(surface);
        refresh_memory_stats();
        return 0;
    }
    surface->old_bitmap = (HBITMAP)SelectObject(surface->dc, surface->bitmap);
    if (!surface->old_bitmap ||
        (HGDIOBJ)surface->old_bitmap == HGDI_ERROR) {
        surface->old_bitmap = NULL;
        render_allocation_note_failure(RENDER_ALLOCATION_PHYSICAL_OVERLAY,
                                       width, height);
        release_surface(surface);
        refresh_memory_stats();
        return 0;
    }
    surface->width = width;
    surface->height = height;
    if (kind == OVERLAY_KIND_RIVER) stats.river_surface_allocations++;
    else stats.wind_surface_allocations++;
    refresh_memory_stats();
    return 1;
}

static void clear_surface(OverlaySurface *surface) {
    uint64_t pixels = (uint64_t)surface->width * (uint64_t)surface->height;
    memset(surface->pixels, 0, (size_t)pixels * sizeof(surface->pixels[0]));
    stats.surface_clear_pixels += pixels;
}

static void finalize_surface_alpha(OverlaySurface *surface) {
    uint64_t count = (uint64_t)surface->width * (uint64_t)surface->height;
    uint64_t i;
    for (i = 0; i < count; i++) {
        unsigned int rgb = surface->pixels[i] & 0x00ffffffu;
        surface->pixels[i] = rgb ? rgb | 0xff000000u : 0u;
    }
    stats.surface_finalize_pixels += count;
}

static int field_available(OverlayKind kind, const RenderSnapshot *snapshot) {
    if (kind == OVERLAY_KIND_RIVER) {
        return snapshot->rivers.valid && snapshot->rivers.map_w == snapshot->map_w &&
               snapshot->rivers.map_h == snapshot->map_h;
    }
    return snapshot->wind.valid && snapshot->wind.map_w == snapshot->map_w &&
           snapshot->wind.map_h == snapshot->map_h;
}

static int begin_surface_pixel_draw(OverlaySurface *surface) {
    int saved = SaveDC(surface->dc);
    if (!saved) return 0;
    SetMapMode(surface->dc, MM_TEXT);
    SetWindowOrgEx(surface->dc, 0, 0, NULL);
    SetViewportOrgEx(surface->dc, 0, 0, NULL);
    return saved;
}

static int ensure_overlay(HDC hdc, const RenderSnapshot *snapshot, int lod,
                          OverlayKind kind) {
    OverlayKey key;
    OverlaySurface *surface;
    RECT logical_client;
    MapLayout logical_layout;
    int path_count = 0;
    int saved;
    if (kind == OVERLAY_KIND_RIVER) stats.river_ensure_calls++;
    else stats.wind_ensure_calls++;
    if (!hdc || !snapshot || !snapshot->world_generated ||
        snapshot->map_w <= 0 || snapshot->map_h <= 0) return 0;
    key = make_key(kind, snapshot, lod);
    surface = find_exact(kind, &key);
    if (surface) {
        if (kind == OVERLAY_KIND_RIVER) stats.river_hits++;
        else stats.wind_hits++;
        return 1;
    }
    if (kind == OVERLAY_KIND_RIVER) stats.river_misses++;
    else stats.wind_misses++;
    surface = surface_for(kind, key.lod);
    if (kind == OVERLAY_KIND_WIND) {
        surface->valid = 0;
        surface->item_count = 0;
        if (field_available(kind, snapshot)) {
            if (!wind_render_prepare_lod(hdc, snapshot, key.lod)) return 0;
            surface->item_count = wind_render_stats()->last_sample_count;
            stats.wind_sample_visits += (uint64_t)max(
                0, wind_render_stats()->last_source_sample_count);
        }
        surface->key = key;
        surface->valid = 1;
        stats.wind_rebuilds++;
        refresh_memory_stats();
        return 1;
    }
    if (!field_available(kind, snapshot)) {
        stats.river_build_failures++;
        refresh_memory_stats();
        return 0;
    }
    if (!ensure_surface(hdc, surface, snapshot->map_w * key.scale,
                        snapshot->map_h * key.scale, kind)) {
        stats.river_build_failures++;
        return 0;
    }
    surface->valid = 0;
    surface->item_count = 0;
    surface->target_count = 0;
    surface->connected_count = 0;
    clear_surface(surface);
    logical_client = (RECT){0, 0, surface->width, surface->height};
    logical_layout = (MapLayout){0, 0, key.scale,
                                 surface->width, surface->height};
    saved = begin_surface_pixel_draw(surface);
    if (!saved) {
        stats.river_build_failures++;
        refresh_memory_stats();
        return 0;
    }
    {
        const HydrologyRenderStats *river_stats;
        if (!river_render_draw_layer_lod(surface->dc, logical_client,
                                         logical_layout, snapshot, key.lod)) {
            RestoreDC(surface->dc, saved);
            stats.river_build_failures++;
            refresh_memory_stats();
            return 0;
        }
        river_geometry_paths(&path_count);
        river_stats = river_geometry_stats();
        surface->item_count = river_stats ?
            river_stats->visible_river_count_last_draw : 0;
        surface->target_count = river_stats ?
            river_stats->lod_target_last_draw : 0;
        surface->connected_count = river_stats ?
            river_stats->lod_connected_path_count_last_draw : 0;
    }
    stats.river_rebuilds++;
    stats.river_path_visits += (uint64_t)max(0, path_count);
    if (!RestoreDC(surface->dc, saved)) {
        stats.river_build_failures++;
        refresh_memory_stats();
        return 0;
    }
    finalize_surface_alpha(surface);
    surface->key = key;
    surface->valid = 1;
    refresh_memory_stats();
    return 1;
}

int render_static_physical_overlay_cache_ensure_river(
    HDC hdc, const RenderSnapshot *snapshot, int lod) {
    return ensure_overlay(hdc, snapshot, lod, OVERLAY_KIND_RIVER);
}

int render_static_physical_overlay_cache_ensure_wind(
    HDC hdc, const RenderSnapshot *snapshot, int lod) {
    return ensure_overlay(hdc, snapshot, lod, OVERLAY_KIND_WIND);
}

int render_static_physical_overlay_cache_prewarm_all(
    HDC hdc, const RenderSnapshot *snapshot) {
    DWORD start = GetTickCount();
    int ok = hdc && snapshot && snapshot->world_generated;
    int lod;
    stats.prewarm_attempts++;
    for (lod = 0; lod < RENDER_STATIC_OVERLAY_RIVER_LOD_COUNT; lod++) {
        if (!render_static_physical_overlay_cache_ensure_river(hdc, snapshot, lod)) ok = 0;
    }
    for (lod = 0; lod < RENDER_STATIC_OVERLAY_WIND_LOD_COUNT; lod++) {
        if (!render_static_physical_overlay_cache_ensure_wind(hdc, snapshot, lod)) ok = 0;
    }
    stats.prewarm_last_ms = (int)(GetTickCount() - start);
    refresh_memory_stats();
    if (render_static_physical_overlay_cache_river_ready_mask(snapshot) != 0x0fu ||
        render_static_physical_overlay_cache_wind_ready_mask(snapshot) != 0x07u) ok = 0;
    if (ok) stats.prewarm_completions++;
    return ok;
}

static int overlay_ready(OverlayKind kind, const RenderSnapshot *snapshot, int lod) {
    OverlayKey key;
    if (!snapshot || !snapshot->world_generated) return 0;
    key = make_key(kind, snapshot, lod);
    return find_exact(kind, &key) != NULL;
}

int render_static_physical_overlay_cache_river_ready(
    const RenderSnapshot *snapshot, int lod) {
    return overlay_ready(OVERLAY_KIND_RIVER, snapshot, lod);
}

int render_static_physical_overlay_cache_wind_ready(
    const RenderSnapshot *snapshot, int lod) {
    return overlay_ready(OVERLAY_KIND_WIND, snapshot, lod);
}

unsigned int render_static_physical_overlay_cache_river_ready_mask(
    const RenderSnapshot *snapshot) {
    unsigned int mask = 0;
    int lod;
    for (lod = 0; lod < RENDER_STATIC_OVERLAY_RIVER_LOD_COUNT; lod++) {
        if (overlay_ready(OVERLAY_KIND_RIVER, snapshot, lod)) mask |= 1u << lod;
    }
    return mask;
}

unsigned int render_static_physical_overlay_cache_wind_ready_mask(
    const RenderSnapshot *snapshot) {
    unsigned int mask = 0;
    int lod;
    for (lod = 0; lod < RENDER_STATIC_OVERLAY_WIND_LOD_COUNT; lod++) {
        if (overlay_ready(OVERLAY_KIND_WIND, snapshot, lod)) mask |= 1u << lod;
    }
    return mask;
}

static void present_overlay(HDC destination, RECT client, MapLayout layout,
                            const RenderSnapshot *snapshot, int lod,
                            OverlayKind kind) {
    OverlayKey key;
    OverlaySurface *surface;
    RECT viewport;
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    int saved;
    if (!destination || !snapshot || !snapshot->world_generated ||
        layout.draw_w <= 0 || layout.draw_h <= 0) return;
    key = make_key(kind, snapshot, lod);
    surface = find_exact(kind, &key);
    if (!surface) {
        if (kind == OVERLAY_KIND_RIVER) stats.river_present_misses++;
        else stats.wind_present_misses++;
        return;
    }
    if (kind == OVERLAY_KIND_RIVER) {
        stats.river_present_hits++;
        stats.selected_river_lod = key.lod;
        stats.selected_river_visible = surface->item_count;
        stats.selected_river_target = surface->target_count;
        stats.selected_river_connected = surface->connected_count;
        stats.selected_river_scale = key.scale;
    } else {
        stats.wind_present_hits++;
        stats.selected_wind_lod = key.lod;
        stats.selected_wind_samples = surface->item_count;
        stats.selected_wind_scale = key.scale;
        stats.wind_anchor_visits += (uint64_t)max(0, surface->item_count);
        stats.wind_sprite_blits += wind_render_present_prepared_lod(
            destination, client, layout, snapshot, key.lod);
        return;
    }
    viewport = get_map_content_rect(client);
    saved = SaveDC(destination);
    IntersectClipRect(destination, viewport.left, viewport.top,
                      viewport.right, viewport.bottom);
    AlphaBlend(destination, layout.map_x, layout.map_y,
               layout.draw_w, layout.draw_h,
               surface->dc, 0, 0, surface->width, surface->height, blend);
    RestoreDC(destination, saved);
}

void render_static_physical_overlay_cache_present_river(
    HDC destination, RECT client, MapLayout layout,
    const RenderSnapshot *snapshot, int lod) {
    present_overlay(destination, client, layout, snapshot, lod, OVERLAY_KIND_RIVER);
}

void render_static_physical_overlay_cache_present_wind(
    HDC destination, RECT client, MapLayout layout,
    const RenderSnapshot *snapshot, int lod) {
    present_overlay(destination, client, layout, snapshot, lod, OVERLAY_KIND_WIND);
}

const RenderStaticPhysicalOverlayCacheStats *
render_static_physical_overlay_cache_stats(void) {
    refresh_memory_stats();
    return &stats;
}

void render_static_physical_overlay_cache_reset_debug_counters(void) {
    memset(&stats, 0, sizeof(stats));
    refresh_memory_stats();
}

void render_static_physical_overlay_cache_invalidate(void) {
    int invalidations = stats.invalidations + 1;
    int i;
    for (i = 0; i < RENDER_STATIC_OVERLAY_RIVER_LOD_COUNT; i++)
        release_surface(&river_entries[i]);
    for (i = 0; i < RENDER_STATIC_OVERLAY_WIND_LOD_COUNT; i++)
        release_surface(&wind_entries[i]);
    river_geometry_release();
    wind_render_invalidate_geometry();
    memset(&stats, 0, sizeof(stats));
    stats.invalidations = invalidations;
}
