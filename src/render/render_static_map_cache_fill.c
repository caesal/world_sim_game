#include "render/render_static_map_cache_internal.h"

#include "core/game_types.h"
#include "render/map_display_policy.h"
#include "render/map_ownership_surface.h"

#include <string.h>

static unsigned int fill_pixel_from_color(COLORREF color, int alpha) {
    int r = GetRValue(color);
    int g = GetGValue(color);
    int b = GetBValue(color);
    alpha = clamp(alpha, 0, 255);
    if (alpha < 255) {
        r = r * alpha / 255;
        g = g * alpha / 255;
        b = b * alpha / 255;
    }
    return (unsigned int)b | ((unsigned int)g << 8) |
           ((unsigned int)r << 16) | ((unsigned int)alpha << 24);
}

static void clear_fill_pixels(MapLayerCache *cache, unsigned int value) {
    int total = cache->width * cache->height;
    int i;
    for (i = 0; i < total; i++) cache->pixels[i] = value;
}

static void fill_tile_pixels(MapLayerCache *cache, int tile_x, int tile_y, unsigned int value) {
    int x0 = tile_x * MAP_LAYER_CACHE_SCALE;
    int y0 = tile_y * MAP_LAYER_CACHE_SCALE;
    int y;
    int x;
    for (y = 0; y < MAP_LAYER_CACHE_SCALE && y0 + y < cache->height; y++) {
        unsigned int *row = &cache->pixels[(y0 + y) * cache->width + x0];
        for (x = 0; x < MAP_LAYER_CACHE_SCALE && x0 + x < cache->width; x++) row[x] = value;
    }
}

static int build_owner_surface_fill(MapLayerCache *cache, const RenderSnapshot *snapshot, int live) {
    MapOwnershipSurfaceView surface;
    MapDisplayFillPolicy fills[MAX_CIVS];
    unsigned int pixels[MAX_CIVS];
    int active[MAX_CIVS];
    int civ_total;
    int x;
    int y;
    if (display_mode == DISPLAY_REGIONS) return 0;
    if (!(live ? map_ownership_surface_live_view(&surface) :
          map_ownership_surface_snapshot_view(snapshot, &surface))) return 0;
    civ_total = clamp(live ? civ_count : (snapshot ? snapshot->civ_count : 0), 0, MAX_CIVS);
    memset(active, 0, sizeof(active));
    for (x = 0; x < civ_total; x++) {
        int ok = live ? map_display_policy_live_owner_fill(x, display_mode, &fills[x]) :
                 map_display_policy_snapshot_owner_fill(snapshot, x, display_mode, &fills[x]);
        active[x] = ok && fills[x].active;
        pixels[x] = active[x] ? fill_pixel_from_color(fills[x].color, fills[x].alpha) : 0;
    }
    for (y = 0; y < surface.height; y++) {
        for (x = 0; x < surface.width; x++) {
            int owner = surface.owner[y * surface.width + x];
            if (owner >= 0 && owner < civ_total && active[owner]) {
                fill_tile_pixels(cache, x, y, pixels[owner]);
            }
        }
    }
    return 1;
}

void render_static_map_cache_build_fill_pixels(MapLayerCache *cache,
                                               const RenderSnapshot *snapshot,
                                               int live) {
    int width;
    int height;
    int x;
    int y;
    clear_fill_pixels(cache, 0);
    if (!snapshot || !map_display_policy_requires_fill_layer(display_mode)) return;
    if (build_owner_surface_fill(cache, snapshot, live)) return;
    width = live ? map_w : snapshot->map_w;
    height = live ? map_h : snapshot->map_h;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            const SnapshotTile *tile = &snapshot->tiles[y * snapshot->map_w + x];
            MapDisplayFillPolicy fill;
            if (live) {
                if (!map_display_policy_live_fill(x, y, display_mode, &fill)) continue;
            } else if (!map_display_policy_snapshot_fill(snapshot, tile, display_mode, &fill)) continue;
            fill_tile_pixels(cache, x, y, fill_pixel_from_color(fill.color, fill.alpha));
        }
    }
}
