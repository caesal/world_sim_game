#include "render/render_ocean_decoration.h"

#include "render/render_common.h"
#include "render/render_ocean_assets.h"
#include "render/render_ocean_decoration_items.h"
#include "render/render_layer_cache.h"
#include "render/render_water_surface_cache.h"

#define OCEAN_NORM 10000
#define OCEAN_NOMINAL_W 1100
#define OCEAN_NOMINAL_H 620

static int exterior_rebuilds, exterior_texture_score;
static LayerCache exterior_cache;

static unsigned int mix_u32(unsigned int h, unsigned int v) {
    h ^= v + 0x9e3779b9u + (h << 6) + (h >> 2);
    return h ? h : 2166136261u;
}

static int motif_height_for_width(unsigned char type, int width) {
    const OceanMotifAssetInfo *info = ocean_assets_motif_info(type);
    if (!info || info->default_w <= 0) return max(18, width * 3 / 4);
    return max(18, (int)((long long)width * info->default_h / info->default_w));
}

static COLORREF ocean_base_color(void) {
    return RGB(54, 126, 184);
}

static unsigned int exterior_texture_key(void) {
    return mix_u32(2166136261u,
                   (unsigned int)ocean_assets_texture_loaded());
}

static int cache_matches_key(const LayerCache *cache, unsigned int key) {
    return cache->valid && cache->key == key &&
           cache->width == OCEAN_NOMINAL_W &&
           cache->height == OCEAN_NOMINAL_H;
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

static int prepare_exterior_cache(HDC hdc, RECT client, MapLayout layout,
                                  const RenderSnapshot *snapshot,
                                  int allow_build) {
    const OceanDecorationItemSet *items;
    RECT canonical = {0, 0, OCEAN_NOMINAL_W, OCEAN_NOMINAL_H};
    MapLayout canonical_layout = {0, 0, 1, OCEAN_NOMINAL_W,
                                  OCEAN_NOMINAL_H};
    unsigned int texture_key;
    unsigned int key;
    int i;
    int vw = OCEAN_NOMINAL_W, vh = OCEAN_NOMINAL_H;
    (void)client;
    (void)layout;
    if (allow_build) {
        if (!ocean_assets_texture_ready() ||
            !ocean_assets_motifs_ready()) return 0;
        items = render_ocean_decoration_items_ensure(snapshot);
    } else {
        items = render_ocean_decoration_items_ready(snapshot);
    }
    if (!items) {
        return 0;
    }
    texture_key = exterior_texture_key();
    key = mix_u32(items->key, texture_key);
    if (!cache_matches_key(&exterior_cache, key)) {
        if (!allow_build) return 0;
        if (!render_layer_cache_ensure(hdc, &exterior_cache, canonical,
                                       canonical_layout, 0, 0)) {
            return 0;
        }
        fill_rect(exterior_cache.dc, canonical, ocean_base_color());
        if (!ocean_assets_draw_texture(exterior_cache.dc, canonical)) {
            exterior_cache.valid = 0;
            exterior_texture_score = 0;
            return 0;
        }
        exterior_texture_score = 900;
        for (i = 0; i < items->exterior_count; i++) {
            const OceanDecorationItem *item = &items->exterior_items[i];
            int x = item->x * vw / OCEAN_NORM;
            int y = item->y * vh / OCEAN_NORM;
            int w = max(18, item->size * vw / OCEAN_NOMINAL_W);
            int h = motif_height_for_width(item->type, w);
            RECT dst = {x - w / 2, y - h / 2, x + w / 2, y + h / 2};
            if (!ocean_assets_draw_motif(
                    exterior_cache.dc, item->type, dst)) {
                exterior_cache.valid = 0;
                return 0;
            }
        }
        exterior_cache.key = key;
        exterior_cache.valid = 1;
        exterior_rebuilds++;
    }
    return 1;
}

static void draw_exterior_cache(HDC hdc, RECT client, MapLayout layout,
                                const RenderSnapshot *snapshot) {
    RECT viewport = get_map_viewport_rect(client);
    int width = viewport.right - viewport.left;
    int height = viewport.bottom - viewport.top;
    RECT source = exterior_cover_source(width, height);
    if (!prepare_exterior_cache(hdc, client, layout, snapshot, 0)) {
        fill_rect(hdc, viewport, ocean_base_color());
        return;
    }
    if (width <= 0 || height <= 0) return;
    {
        int saved = SaveDC(hdc);
        SetStretchBltMode(hdc, HALFTONE);
        SetBrushOrgEx(hdc, viewport.left, viewport.top, NULL);
        StretchBlt(hdc, viewport.left, viewport.top, width, height,
                   exterior_cache.dc, source.left, source.top,
                   source.right - source.left,
                   source.bottom - source.top, SRCCOPY);
        RestoreDC(hdc, saved);
    }
}

int render_ocean_decoration_prewarm_background(
    HDC hdc, RECT client, MapLayout layout,
    const RenderSnapshot *snapshot) {
    return prepare_exterior_cache(hdc, client, layout, snapshot, 1);
}

void render_ocean_decoration_draw_background(HDC hdc, RECT client, MapLayout layout,
                                             const RenderSnapshot *snapshot) {
    if (!snapshot || !snapshot->world_generated) return;
    draw_exterior_cache(hdc, client, layout, snapshot);
}

void render_ocean_decoration_draw(HDC hdc, RECT client, MapLayout layout,
                                  const RenderSnapshot *snapshot) {
    render_ocean_decoration_draw_background(hdc, client, layout, snapshot);
    render_water_surface_cache_present(hdc, client, layout, snapshot);
}

RenderLayerCacheMemory render_ocean_decoration_memory(void) {
    RenderLayerCacheMemory total = render_layer_cache_memory(&exterior_cache);
    render_layer_cache_memory_add(&total, render_water_surface_cache_memory());
    return total;
}

void render_ocean_decoration_reset_debug(void) {
    exterior_cache.valid = 0;
    render_water_surface_cache_invalidate();
    render_water_surface_cache_reset_debug();
    render_ocean_decoration_items_reset();
    exterior_texture_score = 0;
    exterior_rebuilds = 0;
}

OceanDecorationProbeInfo render_ocean_decoration_probe_info(void) {
    const OceanDecorationItemSet *items =
        render_ocean_decoration_items_ensure(NULL);
    const RenderWaterSurfaceCacheStats *water =
        render_water_surface_cache_stats();
    OceanDecorationProbeInfo info;
    info.exterior_items = items->exterior_count; info.interior_items = items->interior_count;
    info.exterior_rebuilds = exterior_rebuilds; info.interior_rebuilds = water->rebuilds;
    info.item_rebuilds = items->item_rebuilds; info.compass_items = 0;
    info.interior_water_only = water->interior_water_only;
    info.interior_deep_only = water->interior_deep_only;
    info.interior_shallow_allowed_seen = water->interior_shallow_allowed_seen;
    info.same_type_spacing_ok = items->same_type_spacing_ok;
    info.interior_min_clearance = water->interior_min_clearance;
    info.motif_overlap_count = items->motif_overlap_count;
    info.motif_spacing_violation_count = items->motif_spacing_violation_count;
    info.exterior_spacing_ok = items->exterior_spacing_ok;
    info.exterior_texture_score = exterior_texture_score;
    info.interior_texture_score = water->texture_score;
    info.texture_asset_ready = ocean_assets_texture_loaded();
    info.motif_asset_ready = ocean_assets_motifs_loaded();
    info.primitive_wave_stamps = 0;
    info.coverage_rebuilds = water->rebuilds;
    info.coverage_row_spans = water->water_row_spans;
    info.coverage_ocean_tiles = water->ocean_tiles;
    info.coverage_lake_tiles_excluded = water->lake_tiles;
    info.coverage_uses_color_key = 0;
    info.item_hash = items->hash;
    info.motif_mask = items->motif_mask;
    return info;
}
