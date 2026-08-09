#include "render/render_ocean_decoration.h"

#include "core/constants.h"
#include "render/render_common.h"
#include "render/render_ocean_assets.h"
#include "render/render_ocean_decoration_cache.h"
#include "render/render_ocean_decoration_items.h"
#include "render/render_ocean_decoration_rules.h"
#include "render/render_ocean_decoration_water.h"
#include "render/render_ocean_texture.h"
#include "render/render_water_surface_cache.h"

#include <string.h>

enum {
    OCEAN_NORM = 10000,
    OCEAN_NOMINAL_W = 1100,
    OCEAN_NOMINAL_H = 620,
    OCEAN_EXTERIOR_MAX = 64,
    OCEAN_INTERIOR_MAX = 20
};

OceanDecorationDebugStats ocean_decoration_debug_stats;

static unsigned int mix_u32(unsigned int h, unsigned int v) {
    h ^= v + 0x9e3779b9u + (h << 6) + (h >> 2);
    return h ? h : 2166136261u;
}

static int motif_height_for_width(unsigned char type, int width) {
    const OceanMotifAssetInfo *info = ocean_assets_motif_info(type);
    if (!info || info->default_w <= 0) return max(18, width * 3 / 4);
    return max(18, (int)((long long)width * info->default_h /
                         info->default_w));
}

static int motif_source_height(unsigned char type, int width) {
    const OceanMotifAssetInfo *info = ocean_assets_motif_info(type);
    if (!info || info->default_w <= 0) return max(1, width * 3 / 4);
    return max(1, (int)((long long)width * info->default_h /
                        info->default_w));
}

static RECT exterior_cover_source(int destination_w, int destination_h) {
    RECT source = {0, 0, OCEAN_NOMINAL_W, OCEAN_NOMINAL_H};
    if (destination_w <= 0 || destination_h <= 0) return source;
    if ((long long)destination_w * OCEAN_NOMINAL_H >
        (long long)destination_h * OCEAN_NOMINAL_W) {
        int height = (int)(((long long)OCEAN_NOMINAL_W * destination_h +
                            destination_w / 2) / destination_w);
        height = clamp(height, 1, OCEAN_NOMINAL_H);
        source.top = (OCEAN_NOMINAL_H - height) / 2;
        source.bottom = source.top + height;
    } else {
        int width = (int)(((long long)OCEAN_NOMINAL_H * destination_w +
                           destination_h / 2) / destination_h);
        width = clamp(width, 1, OCEAN_NOMINAL_W);
        source.left = (OCEAN_NOMINAL_W - width) / 2;
        source.right = source.left + width;
    }
    return source;
}

static RECT visible_map_rect(RECT client, MapLayout layout) {
    RECT map = {layout.map_x, layout.map_y,
                layout.map_x + layout.draw_w,
                layout.map_y + layout.draw_h};
    RECT viewport = get_map_viewport_rect(client);
    RECT visible = {0};
    IntersectRect(&visible, &map, &viewport);
    return visible;
}

static void sync_debug_stats(void) {
    const OceanDecorationCacheStats *stats = ocean_decoration_cache_stats();
    ocean_decoration_debug_stats.exterior_layer_rebuilds =
        stats->exterior_rebuilds;
    ocean_decoration_debug_stats.interior_layer_rebuilds =
        stats->interior_rebuilds;
    ocean_decoration_debug_stats.exterior_layer_presents =
        stats->exterior_presents;
    ocean_decoration_debug_stats.interior_layer_presents =
        stats->interior_presents;
    ocean_decoration_debug_stats.exterior_layer_allocations =
        stats->exterior_allocations;
    ocean_decoration_debug_stats.interior_layer_allocations =
        stats->interior_allocations;
    ocean_decoration_debug_stats.exterior_layer_clears =
        stats->exterior_clears;
    ocean_decoration_debug_stats.interior_layer_clears =
        stats->interior_clears;
    ocean_decoration_debug_stats.exterior_cleared_pixels =
        stats->exterior_cleared_pixels;
    ocean_decoration_debug_stats.interior_cleared_pixels =
        stats->interior_cleared_pixels;
    ocean_decoration_debug_stats.exterior_retained_bytes =
        stats->exterior_retained_bytes;
    ocean_decoration_debug_stats.interior_retained_bytes =
        stats->interior_retained_bytes;
    ocean_decoration_debug_stats.exterior_surface_identity =
        stats->exterior_surface_identity;
    ocean_decoration_debug_stats.interior_surface_identity =
        stats->interior_surface_identity;
    ocean_decoration_debug_stats.exterior_layer_key = stats->exterior_key;
    ocean_decoration_debug_stats.interior_layer_key = stats->interior_key;
}

static RECT exterior_layer_rect(int client_w, int client_h) {
    RECT rect = {0, TOP_BAR_H, client_w, client_h - BOTTOM_BAR_H};
    if (rect.right < rect.left + 80) rect.right = rect.left + 80;
    if (rect.bottom < rect.top + 80) rect.bottom = rect.top + 80;
    return rect;
}

static unsigned int exterior_key(int client_w, int client_h,
                                 const OceanDecorationItemSet *items) {
    unsigned int key = mix_u32(items->key, items->exterior_hash);
    key = mix_u32(key, ocean_assets_motif_identity());
    key = mix_u32(key, (unsigned int)items->exterior_count);
    key = mix_u32(key, (unsigned int)client_w);
    return mix_u32(key, (unsigned int)client_h);
}

static unsigned int interior_key(const RenderSnapshot *snapshot,
                                 const OceanDecorationItemSet *items) {
    unsigned int key = mix_u32(items->key, items->interior_hash);
    key = mix_u32(key, ocean_assets_motif_identity());
    key = mix_u32(key, (unsigned int)items->interior_count);
    key = mix_u32(key, (unsigned int)snapshot->map_w);
    return mix_u32(key, (unsigned int)snapshot->map_h);
}

static int build_exterior_layer(OceanDecorationLayer *layer, RECT viewport,
                                const OceanDecorationItemSet *items) {
    OceanMotifRasterItem draws[OCEAN_EXTERIOR_MAX];
    int vw = viewport.right - viewport.left;
    int vh = viewport.bottom - viewport.top;
    RECT source = exterior_cover_source(vw, vh);
    int sw = source.right - source.left;
    int sh = source.bottom - source.top;
    int i;
    if (vw <= 0 || vh <= 0 || sw <= 0 || sh <= 0 ||
        items->exterior_count > OCEAN_EXTERIOR_MAX) return 0;
    for (i = 0; i < items->exterior_count; i++) {
        const OceanDecorationItem *item = &items->exterior_items[i];
        int canonical_x = item->x * OCEAN_NOMINAL_W / OCEAN_NORM;
        int canonical_y = item->y * OCEAN_NOMINAL_H / OCEAN_NORM;
        int canonical_w = max(18, item->size);
        int canonical_h = motif_height_for_width(item->type, canonical_w);
        int x = viewport.left + (canonical_x - source.left) * vw / sw;
        int y = viewport.top + (canonical_y - source.top) * vh / sh;
        int width = max(18, canonical_w * vw / sw);
        int height = max(18, canonical_h * vh / sh);
        draws[i].index = item->type;
        draws[i].dst = (RECT){x - width / 2, y - height / 2,
                              x + width / 2, y + height / 2};
    }
    return ocean_assets_draw_motif_layer(
        layer->pixels, layer->width, layer->height,
        draws, items->exterior_count);
}

static int build_interior_layer(OceanDecorationLayer *layer,
                                const RenderSnapshot *snapshot,
                                const OceanDecorationItemSet *items) {
    OceanMotifRasterItem draws[OCEAN_INTERIOR_MAX];
    int i;
    if (items->interior_count > OCEAN_INTERIOR_MAX) return 0;
    for (i = 0; i < items->interior_count; i++) {
        const OceanDecorationItem *item = &items->interior_items[i];
        int x = item->x * layer->width / snapshot->map_w;
        int y = item->y * layer->height / snapshot->map_h;
        int width = max(4, item->size * layer->width / 900);
        int height = motif_source_height(item->type, width);
        draws[i].index = item->type;
        draws[i].dst = (RECT){x - width / 2, y - height / 2,
                              x + width / 2, y + height / 2};
    }
    return ocean_assets_draw_motif_layer(
        layer->pixels, layer->width, layer->height,
        draws, items->interior_count);
}

static OceanDecorationLayer *ensure_layer(
    HDC hdc, OceanDecorationLayerKind kind, int width, int height,
    unsigned int key, RECT viewport, const RenderSnapshot *snapshot,
    const OceanDecorationItemSet *items, int allow_build) {
    OceanDecorationLayer *layer = ocean_decoration_cache_find(
        kind, width, height, key);
    int built;
    if (layer || !allow_build) return layer;
    layer = ocean_decoration_cache_prepare(hdc, kind, width, height);
    if (!layer || !ocean_decoration_cache_clear(kind, layer)) return NULL;
    built = kind == OCEAN_DECORATION_LAYER_EXTERIOR
        ? build_exterior_layer(layer, viewport, items)
        : build_interior_layer(layer, snapshot, items);
    if (!built) return NULL;
    ocean_decoration_cache_commit(kind, layer, key);
    return layer;
}

static int ensure_layers(HDC hdc, RECT client,
                         const RenderSnapshot *snapshot, int allow_build,
                         OceanDecorationLayer **exterior,
                         OceanDecorationLayer **interior) {
    const OceanDecorationItemSet *items;
    int client_w = client.right - client.left;
    int client_h = client.bottom - client.top;
    RECT layer_viewport = exterior_layer_rect(client_w, client_h);
    if (allow_build) {
        if (!ocean_assets_motifs_ready()) return 0;
        items = render_ocean_decoration_items_ensure(snapshot);
    } else {
        if (!ocean_assets_motifs_loaded()) return 0;
        items = render_ocean_decoration_items_ready(snapshot);
    }
    if (!items || snapshot->map_w <= 0 || snapshot->map_h <= 0) return 0;
    ocean_decoration_debug_stats.interior_water_only =
        items->interior_water_only;
    ocean_decoration_debug_stats.interior_deep_only =
        items->interior_deep_only;
    ocean_decoration_debug_stats.interior_shallow_allowed_seen =
        items->interior_shallow_allowed_seen;
    ocean_decoration_debug_stats.interior_min_clearance =
        items->interior_min_clearance;
    *exterior = ensure_layer(
        hdc, OCEAN_DECORATION_LAYER_EXTERIOR, client_w, client_h,
        exterior_key(client_w, client_h, items), layer_viewport,
        snapshot, items, allow_build);
    *interior = ensure_layer(
        hdc, OCEAN_DECORATION_LAYER_INTERIOR,
        MAX_MAP_W, MAX_MAP_H,
        interior_key(snapshot, items), layer_viewport,
        snapshot, items, allow_build);
    sync_debug_stats();
    return *exterior && *interior;
}

static int present_exterior(HDC hdc, RECT client, RECT map,
                            const OceanDecorationLayer *layer) {
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    RECT viewport = get_map_viewport_rect(client);
    int saved = SaveDC(hdc);
    int ok;
    if (!saved) return 0;
    if (IntersectClipRect(hdc, viewport.left, viewport.top,
                          viewport.right, viewport.bottom) == ERROR ||
        (!IsRectEmpty(&map) && ExcludeClipRect(
            hdc, map.left, map.top, map.right, map.bottom) == ERROR)) {
        RestoreDC(hdc, saved);
        return 0;
    }
    ok = AlphaBlend(hdc, client.left, client.top,
                    client.right - client.left, client.bottom - client.top,
                    layer->dc, 0, 0, layer->width, layer->height, blend);
    RestoreDC(hdc, saved);
    if (ok) ocean_decoration_cache_note_present(
        OCEAN_DECORATION_LAYER_EXTERIOR);
    return ok;
}

static int present_interior(HDC hdc, RECT client, MapLayout layout,
                            const OceanDecorationLayer *layer) {
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    RECT map = visible_map_rect(client, layout);
    int saved;
    int ok;
    if (IsRectEmpty(&map)) return 1;
    saved = SaveDC(hdc);
    if (!saved) return 0;
    if (IntersectClipRect(hdc, map.left, map.top,
                          map.right, map.bottom) == ERROR) {
        RestoreDC(hdc, saved);
        return 0;
    }
    SetStretchBltMode(hdc, COLORONCOLOR);
    ok = AlphaBlend(hdc, layout.map_x, layout.map_y,
                    layout.draw_w, layout.draw_h,
                    layer->dc, 0, 0, layer->width, layer->height, blend);
    RestoreDC(hdc, saved);
    if (ok) ocean_decoration_cache_note_present(
        OCEAN_DECORATION_LAYER_INTERIOR);
    return ok;
}

static int ensure_background(HDC hdc, RECT client, MapLayout layout,
                             const RenderSnapshot *snapshot,
                             int allow_build) {
    OceanDecorationLayer *exterior = NULL;
    OceanDecorationLayer *interior = NULL;
    RECT viewport = get_map_viewport_rect(client);
    RECT map = visible_map_rect(client, layout);
    if (!render_ocean_texture_ensure(hdc, client)) return 0;
    if (!render_ocean_texture_copy(hdc, viewport)) return 0;
    if (!snapshot || !snapshot->world_generated) return 1;
    if (!ensure_layers(hdc, client, snapshot, allow_build,
                       &exterior, &interior)) return 1;
    if (!present_exterior(hdc, client, map, exterior) ||
        !present_interior(hdc, client, layout, interior)) return 0;
    sync_debug_stats();
    return 1;
}

int render_ocean_decoration_prewarm_background(
    HDC hdc, RECT client, MapLayout layout,
    const RenderSnapshot *snapshot) {
    return ensure_background(hdc, client, layout, snapshot, 1);
}

void render_ocean_decoration_draw_background(
    HDC hdc, RECT client, MapLayout layout,
    const RenderSnapshot *snapshot) {
    RECT viewport = get_map_viewport_rect(client);
    if (!ensure_background(hdc, client, layout, snapshot, 1))
        fill_rect(hdc, viewport, RGB(55, 135, 199));
}

void render_ocean_decoration_draw(HDC hdc, RECT client, MapLayout layout,
                                  const RenderSnapshot *snapshot) {
    render_ocean_decoration_draw_background(hdc, client, layout, snapshot);
    render_water_surface_cache_present_lake(hdc, client, layout, snapshot);
}

RenderLayerCacheMemory render_ocean_decoration_memory(void) {
    const OceanDecorationCacheStats *cache_stats =
        ocean_decoration_cache_stats();
    RenderLayerCacheMemory total = render_ocean_texture_memory();
    RenderLayerCacheMemory layers = {0};
    layers.bitmaps = cache_stats->persistent_bitmaps;
    layers.dcs = cache_stats->persistent_dcs;
    layers.bitmap_bytes = cache_stats->persistent_bitmap_bytes;
    render_layer_cache_memory_add(&total, layers);
    render_layer_cache_memory_add(&total, render_water_surface_cache_memory());
    return total;
}

void render_ocean_decoration_reset_debug(void) {
    render_ocean_texture_invalidate();
    render_ocean_texture_reset_debug();
    ocean_assets_reset_debug();
    ocean_decoration_cache_invalidate();
    ocean_decoration_cache_reset_debug();
    render_water_surface_cache_invalidate();
    render_water_surface_cache_reset_debug();
    render_ocean_decoration_items_reset();
    ocean_decoration_water_reset_debug();
    memset(&ocean_decoration_debug_stats, 0,
           sizeof(ocean_decoration_debug_stats));
    ocean_decoration_debug_stats.interior_water_only = 1;
    ocean_decoration_debug_stats.interior_deep_only = 1;
    ocean_decoration_debug_stats.interior_min_clearance = 99;
    sync_debug_stats();
}

OceanDecorationProbeInfo render_ocean_decoration_probe_info(void) {
    const OceanDecorationItemSet *items =
        render_ocean_decoration_items_ensure(NULL);
    const RenderWaterSurfaceCacheStats *water =
        render_water_surface_cache_stats();
    const OceanDecorationCacheStats *cache =
        ocean_decoration_cache_stats();
    OceanDecorationProbeInfo info;
    memset(&info, 0, sizeof(info));
    info.exterior_items = items->exterior_count;
    info.interior_items = items->interior_count;
    info.exterior_rebuilds = (int)cache->exterior_rebuilds;
    info.interior_rebuilds = (int)cache->interior_rebuilds;
    info.item_rebuilds = items->item_rebuilds;
    info.interior_water_only =
        ocean_decoration_debug_stats.interior_water_only;
    info.interior_deep_only =
        ocean_decoration_debug_stats.interior_deep_only;
    info.interior_shallow_allowed_seen =
        ocean_decoration_debug_stats.interior_shallow_allowed_seen;
    info.same_type_spacing_ok = items->same_type_spacing_ok;
    info.interior_min_clearance =
        ocean_decoration_debug_stats.interior_min_clearance;
    info.motif_overlap_count = items->motif_overlap_count;
    info.motif_spacing_violation_count =
        items->motif_spacing_violation_count;
    info.exterior_spacing_ok = items->exterior_spacing_ok;
    info.exterior_texture_score = render_ocean_texture_score();
    info.interior_texture_score = render_ocean_texture_score();
    info.texture_asset_ready = ocean_assets_texture_loaded();
    info.motif_asset_ready = ocean_assets_motifs_loaded();
    info.coverage_rebuilds = water->rebuilds;
    info.coverage_row_spans = water->water_row_spans;
    info.coverage_ocean_tiles = water->ocean_tiles;
    info.coverage_lake_tiles_excluded = water->lake_tiles;
    info.item_hash = items->hash;
    info.motif_mask = items->motif_mask;
    return info;
}
