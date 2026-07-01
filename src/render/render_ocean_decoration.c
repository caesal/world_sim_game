#include "render/render_ocean_decoration.h"

#include "render/render_common.h"
#include "render/render_ocean_assets.h"
#include "render/render_ocean_decoration_rules.h"
#include "render/render_ocean_decoration_water.h"
#include "render/render_layer_cache.h"

#include <stdlib.h>

#define OCEAN_EXT_MAX 64
#define OCEAN_INT_MAX 20
#define OCEAN_EXT_TARGET 24
#define OCEAN_INT_TARGET 10
#define OCEAN_NORM 10000
#define OCEAN_NOMINAL_W 1100
#define OCEAN_NOMINAL_H 620

typedef struct {
    unsigned char type;
    unsigned char variant;
    unsigned char interior;
    unsigned char opacity;
    short x;
    short y;
    short size;
} OceanItem;

static OceanItem exterior_items[OCEAN_EXT_MAX];
static OceanItem interior_items[OCEAN_INT_MAX];
static int exterior_count, interior_count, item_rebuilds;
static int exterior_rebuilds, interior_rebuilds, interior_water_only = 1;
static int motif_overlap_count, interior_deep_only = 1;
static int interior_shallow_allowed_seen, same_type_spacing_ok = 1;
static int interior_min_clearance = 99, exterior_texture_score, interior_texture_score;
static unsigned int item_key, item_hash, motif_mask;
static LayerCache exterior_cache, interior_cache;

static unsigned int mix_u32(unsigned int h, unsigned int v) {
    h ^= v + 0x9e3779b9u + (h << 6) + (h >> 2);
    return h ? h : 2166136261u;
}

static unsigned int next_rand(unsigned int *state) {
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static int rand_range(unsigned int *state, int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (int)(next_rand(state) % (unsigned int)(hi - lo + 1));
}

static unsigned int snapshot_seed(const RenderSnapshot *snapshot) {
    unsigned int h = 2166136261u;
    int x, y, step;
    if (!snapshot) return h;
    h = mix_u32(h, (unsigned int)snapshot->map_w);
    h = mix_u32(h, (unsigned int)snapshot->map_h);
    h = mix_u32(h, (unsigned int)snapshot->terrain_revision);
    h = mix_u32(h, (unsigned int)snapshot->region_count);
    step = max(1, (snapshot->map_w + snapshot->map_h) / 22);
    for (y = 0; y < snapshot->map_h; y += step) {
        for (x = (y / step) & 3; x < snapshot->map_w; x += step) {
            const SnapshotTile *t = &snapshot->tiles[y * snapshot->map_w + x];
            h = mix_u32(h, (unsigned int)(t->geography | (t->water_depth << 8) |
                                          (t->elevation << 16)));
        }
    }
    return h;
}

static int far_from_city_and_lane(const RenderSnapshot *snapshot, int x, int y) {
    int i, p;
    for (i = 0; i < snapshot->city_count; i++) {
        int dx, dy;
        if (!snapshot->cities[i].alive) continue;
        dx = snapshot->cities[i].x - x;
        dy = snapshot->cities[i].y - y;
        if (dx * dx + dy * dy < 144) return 0;
    }
    for (i = 0; i < snapshot->lane_count; i++) {
        const SnapshotSeaLane *lane = &snapshot->lanes[i];
        if (!lane->active) continue;
        for (p = 0; p < lane->point_count; p += 3) {
            int dx = lane->points[p].x - x;
            int dy = lane->points[p].y - y;
            if (dx * dx + dy * dy < 100) return 0;
        }
    }
    return 1;
}

static int random_motif_asset(unsigned int *rng, int exterior) {
    int count = ocean_assets_motif_count();
    int total = 0;
    int i, pick;
    for (i = 0; i < count; i++) {
        const OceanMotifAssetInfo *info = ocean_assets_motif_info(i);
        if (!info) continue;
        if (exterior ? !info->allow_exterior : !info->allow_interior) continue;
        total += max(1, info->weight);
    }
    if (total <= 0) return -1;
    pick = (int)(next_rand(rng) % (unsigned int)total);
    for (i = 0; i < count; i++) {
        const OceanMotifAssetInfo *info = ocean_assets_motif_info(i);
        int w;
        if (!info) continue;
        if (exterior ? !info->allow_exterior : !info->allow_interior) continue;
        w = max(1, info->weight);
        if (pick < w) return i;
        pick -= w;
    }
    return -1;
}

static int motif_radius_tiles(unsigned char type, int size) {
    const OceanMotifAssetInfo *info = ocean_assets_motif_info(type);
    int scaled_w = ocean_decoration_motif_scaled_footprint(info, size, 0);
    int scaled_h = ocean_decoration_motif_scaled_footprint(info, size, 1);
    int footprint_radius = max(3, max(scaled_w, scaled_h) / 10), coast_radius;
    coast_radius = info ? max(4, info->min_coast_clear_tiles * 3 / 4) : 5;
    return max(footprint_radius, coast_radius);
}

static int motif_scaled_width(unsigned char type, unsigned int *rng) {
    const OceanMotifAssetInfo *info = ocean_assets_motif_info(type);
    int base = ocean_decoration_motif_base_width(info);
    return rand_range(rng, base * 85 / 100, base * 115 / 100);
}

static int motif_height_for_width(unsigned char type, int width) {
    const OceanMotifAssetInfo *info = ocean_assets_motif_info(type);
    if (!info || info->default_w <= 0) return max(18, width * 3 / 4);
    return max(18, (int)((long long)width * info->default_h / info->default_w));
}

static int collides_exterior(const OceanItem *items, int count, OceanItem item) {
    int i;
    const OceanMotifAssetInfo *info = ocean_assets_motif_info(item.type);
    int w = ocean_decoration_motif_scaled_footprint(info, item.size, 0) * OCEAN_NORM / OCEAN_NOMINAL_W;
    int h = ocean_decoration_motif_scaled_footprint(info, item.size, 1) * OCEAN_NORM / OCEAN_NOMINAL_H;
    for (i = 0; i < count; i++) {
        const OceanMotifAssetInfo *other = ocean_assets_motif_info(items[i].type);
        int margin = ocean_decoration_exterior_spacing_norm(item.type == items[i].type);
        int ow = ocean_decoration_motif_scaled_footprint(other, items[i].size, 0) * OCEAN_NORM / OCEAN_NOMINAL_W;
        int oh = ocean_decoration_motif_scaled_footprint(other, items[i].size, 1) * OCEAN_NORM / OCEAN_NOMINAL_H;
        if (abs(item.x - items[i].x) * 2 < w + ow + margin &&
            abs(item.y - items[i].y) * 2 < h + oh + margin) return 1;
    }
    return 0;
}

static int collides_interior(const OceanItem *items, int count, OceanItem item) {
    int i, r = motif_radius_tiles(item.type, item.size);
    for (i = 0; i < count; i++) {
        int dx = item.x - items[i].x;
        int dy = item.y - items[i].y;
        int sum = r + motif_radius_tiles(items[i].type, items[i].size) +
                  ocean_decoration_interior_spacing_tiles(item.type == items[i].type);
        if (dx * dx + dy * dy < sum * sum) return 1;
    }
    return 0;
}

static void validate_interior_spacing(void) {
    int i, j;
    same_type_spacing_ok = 1;
    for (i = 0; i < interior_count; i++) {
        for (j = i + 1; j < interior_count; j++) {
            int dx = interior_items[i].x - interior_items[j].x;
            int dy = interior_items[i].y - interior_items[j].y;
            int sum = motif_radius_tiles(interior_items[i].type, interior_items[i].size) +
                      motif_radius_tiles(interior_items[j].type, interior_items[j].size) +
                      ocean_decoration_interior_spacing_tiles(interior_items[i].type == interior_items[j].type);
            if (dx * dx + dy * dy < sum * sum) {
                same_type_spacing_ok = 0;
                motif_overlap_count++;
            }
        }
    }
}

static void add_item(OceanItem *items, int *count, int max_count, OceanItem item) {
    if (*count >= max_count) return;
    items[*count] = item;
    (*count)++;
    motif_mask |= 1u << item.type;
    item_hash = mix_u32(item_hash, (unsigned int)(item.type + item.x * 17 + item.y * 131 +
                                                  item.size * 8191 + item.variant));
}

static void rebuild_items(const RenderSnapshot *snapshot) {
    unsigned int rng;
    int attempts;
    item_key = mix_u32(snapshot_seed(snapshot), (unsigned int)ocean_assets_motif_count());
    rng = item_key;
    exterior_count = interior_count = 0;
    item_hash = 2166136261u;
    motif_mask = 0;
    motif_overlap_count = 0;
    same_type_spacing_ok = 1;
    interior_shallow_allowed_seen = 0;
    for (attempts = 0; attempts < 520 && exterior_count < OCEAN_EXT_TARGET; attempts++) {
        OceanItem item;
        const OceanMotifAssetInfo *info;
        int picked = random_motif_asset(&rng, 1);
        if (picked < 0) break;
        info = ocean_assets_motif_info(picked);
        if (!info) continue;
        item.type = (unsigned char)picked;
        item.variant = (unsigned char)rand_range(&rng, 0, 15);
        item.interior = 0;
        item.opacity = (unsigned char)rand_range(&rng, 42, 58);
        item.x = (short)rand_range(&rng, 650, OCEAN_NORM - 650);
        item.y = (short)rand_range(&rng, 650, OCEAN_NORM - 650);
        item.size = (short)motif_scaled_width((unsigned char)picked, &rng);
        if (collides_exterior(exterior_items, exterior_count, item)) {
            continue;
        }
        add_item(exterior_items, &exterior_count, OCEAN_EXT_MAX, item);
    }
    for (attempts = 0; snapshot && attempts < 1400 && interior_count < OCEAN_INT_TARGET; attempts++) {
        int x = rand_range(&rng, 2, snapshot->map_w - 3);
        int y = rand_range(&rng, 2, snapshot->map_h - 3);
        OceanItem item;
        int needed;
        int picked = random_motif_asset(&rng, 0);
        if (picked < 0) break;
        item.type = (unsigned char)picked;
        item.variant = (unsigned char)rand_range(&rng, 0, 15);
        item.interior = 1;
        item.opacity = (unsigned char)rand_range(&rng, 22, 34);
        item.x = (short)x;
        item.y = (short)y;
        item.size = (short)motif_scaled_width((unsigned char)picked, &rng);
        needed = motif_radius_tiles(item.type, item.size);
        if (ocean_decoration_motif_clearance(snapshot, x, y, item.type, needed) < needed) continue;
        if (!far_from_city_and_lane(snapshot, x, y)) continue;
        if (collides_interior(interior_items, interior_count, item)) {
            continue;
        }
        add_item(interior_items, &interior_count, OCEAN_INT_MAX, item);
    }
    item_rebuilds++;
}

static void ensure_items(const RenderSnapshot *snapshot, RECT viewport, MapLayout layout) {
    unsigned int key;
    (void)viewport;
    (void)layout;
    key = mix_u32(snapshot_seed(snapshot), (unsigned int)ocean_assets_motif_count());
    if (key != item_key) rebuild_items(snapshot);
}

static COLORREF ocean_base_color(void) {
    return RGB(54, 126, 184);
}

static void mask_non_water_from_texture(HDC hdc, RECT viewport, MapLayout layout,
                                        const RenderSnapshot *snapshot) {
    RECT map_rect = {layout.map_x, layout.map_y, layout.map_x + layout.draw_w,
                     layout.map_y + layout.draw_h};
    const int edge_bleed = 3;
    int x, y;
    if (!snapshot) return;
    if (map_rect.top - edge_bleed > viewport.top) {
        RECT r = {viewport.left, viewport.top, viewport.right, map_rect.top - edge_bleed};
        fill_rect(hdc, r, RGB(255, 0, 255));
    }
    if (map_rect.bottom + edge_bleed < viewport.bottom) {
        RECT r = {viewport.left, map_rect.bottom + edge_bleed, viewport.right, viewport.bottom};
        fill_rect(hdc, r, RGB(255, 0, 255));
    }
    if (map_rect.left - edge_bleed > viewport.left) {
        RECT r = {viewport.left, map_rect.top, map_rect.left - edge_bleed, map_rect.bottom};
        fill_rect(hdc, r, RGB(255, 0, 255));
    }
    if (map_rect.right + edge_bleed < viewport.right) {
        RECT r = {map_rect.right + edge_bleed, map_rect.top, viewport.right, map_rect.bottom};
        fill_rect(hdc, r, RGB(255, 0, 255));
    }
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            if (ocean_decoration_water_tile(snapshot, x, y)) continue;
            {
                RECT cell = {
                    layout.map_x + (x * layout.draw_w) / snapshot->map_w,
                    layout.map_y + (y * layout.draw_h) / snapshot->map_h,
                    layout.map_x + ((x + 1) * layout.draw_w) / snapshot->map_w + 1,
                    layout.map_y + ((y + 1) * layout.draw_h) / snapshot->map_h + 1
                };
                fill_rect(hdc, cell, RGB(255, 0, 255));
            }
        }
    }
}

static int cache_matches_client_key(const LayerCache *cache, RECT client, unsigned int key) {
    return cache->valid && cache->key == key && cache->width == client.right - client.left &&
           cache->height == client.bottom - client.top;
}

static void draw_exterior_cache(HDC hdc, RECT client, MapLayout layout,
                                const RenderSnapshot *snapshot) {
    RECT viewport = get_map_viewport_rect(client);
    unsigned int key = mix_u32(item_key, (unsigned int)((viewport.right - viewport.left) * 31 +
                              (viewport.bottom - viewport.top) * 17));
    int i;
    int vw = viewport.right - viewport.left, vh = viewport.bottom - viewport.top;
    (void)snapshot;
    if (!cache_matches_client_key(&exterior_cache, client, key)) {
        if (!render_layer_cache_ensure(hdc, &exterior_cache, client, layout, side_panel_w, display_mode)) return;
        if (!ocean_assets_draw_texture(exterior_cache.dc, viewport)) {
            fill_rect(exterior_cache.dc, viewport, ocean_base_color());
        }
        exterior_texture_score = ocean_assets_texture_ready() ? 900 : 330;
        for (i = 0; i < exterior_count; i++) {
            int x = viewport.left + exterior_items[i].x * vw / OCEAN_NORM;
            int y = viewport.top + exterior_items[i].y * vh / OCEAN_NORM;
            int w = max(18, exterior_items[i].size * vw / OCEAN_NOMINAL_W);
            int h = motif_height_for_width(exterior_items[i].type, w);
            RECT dst = {x - w / 2, y - h / 2, x + w / 2, y + h / 2};
            ocean_assets_draw_motif(exterior_cache.dc, exterior_items[i].type, dst);
        }
        exterior_cache.key = key;
        exterior_cache.valid = 1;
        exterior_rebuilds++;
    }
    BitBlt(hdc, viewport.left, viewport.top, viewport.right - viewport.left,
           viewport.bottom - viewport.top, exterior_cache.dc, viewport.left, viewport.top,
           SRCCOPY);
}

static void draw_interior_cache(HDC hdc, RECT client, MapLayout layout,
                                const RenderSnapshot *snapshot) {
    RECT viewport = get_map_viewport_rect(client);
    unsigned int key = mix_u32(item_key, 0x1a7e51u);
    int i;
    if (!snapshot || !snapshot->world_generated) return;
    if (cache_matches_client_key(&interior_cache, client, key)) {
        if (interior_cache.map_x == layout.map_x && interior_cache.map_y == layout.map_y &&
            interior_cache.draw_w == layout.draw_w && interior_cache.draw_h == layout.draw_h) {
            TransparentBlt(hdc, viewport.left, viewport.top, viewport.right - viewport.left,
                           viewport.bottom - viewport.top, interior_cache.dc, viewport.left,
                           viewport.top, viewport.right - viewport.left,
                           viewport.bottom - viewport.top, RGB(255, 0, 255));
        } else {
            render_layer_cache_transparent_map(hdc, client, layout, &interior_cache);
        }
        return;
    }
    {
        if (!render_layer_cache_ensure(hdc, &interior_cache, client, layout, side_panel_w, display_mode)) return;
        render_layer_cache_clear_transparent(&interior_cache);
        interior_water_only = 1;
        interior_deep_only = 1;
        interior_shallow_allowed_seen = 0;
        same_type_spacing_ok = 1;
        interior_min_clearance = 99;
        interior_texture_score = 0;
        if (ocean_assets_draw_texture(interior_cache.dc, viewport)) interior_texture_score = 900;
        validate_interior_spacing();
        for (i = 0; i < interior_count; i++) {
            int needed = motif_radius_tiles(interior_items[i].type, interior_items[i].size);
            const OceanMotifAssetInfo *info = ocean_assets_motif_info(interior_items[i].type);
            int clearance = ocean_decoration_motif_clearance(snapshot, interior_items[i].x,
                                                             interior_items[i].y,
                                                             interior_items[i].type, needed);
            int sx = layout.map_x + (interior_items[i].x * layout.draw_w) / snapshot->map_w;
            int sy = layout.map_y + (interior_items[i].y * layout.draw_h) / snapshot->map_h;
            int w = max(24, interior_items[i].size * layout.draw_w / 900);
            int h = motif_height_for_width(interior_items[i].type, w);
            RECT dst = {sx - w / 2, sy - h / 2, sx + w / 2, sy + h / 2};
            if (ocean_decoration_motif_water_rule(info) == OCEAN_MOTIF_WATER_DEEP_ONLY &&
                !ocean_decoration_deep_ocean_tile(snapshot, interior_items[i].x,
                                                  interior_items[i].y)) interior_deep_only = 0;
            if (ocean_decoration_motif_water_rule(info) == OCEAN_MOTIF_WATER_SHALLOW_OR_DEEP) {
                const SnapshotTile *tile = &snapshot->tiles[interior_items[i].y * snapshot->map_w +
                                                            interior_items[i].x];
                if (tile->geography == GEO_OCEAN && tile->water_depth == WATER_DEPTH_SHALLOW) {
                    interior_shallow_allowed_seen = 1;
                }
            }
            if (clearance < needed) interior_water_only = 0;
            if (clearance < interior_min_clearance) interior_min_clearance = clearance;
            ocean_assets_draw_motif(interior_cache.dc, interior_items[i].type, dst);
        }
        mask_non_water_from_texture(interior_cache.dc, viewport, layout, snapshot);
        interior_cache.key = key;
        interior_cache.valid = 1;
        interior_rebuilds++;
    }
    TransparentBlt(hdc, viewport.left, viewport.top, viewport.right - viewport.left,
                   viewport.bottom - viewport.top, interior_cache.dc, viewport.left,
                   viewport.top, viewport.right - viewport.left,
                   viewport.bottom - viewport.top, RGB(255, 0, 255));
}

void render_ocean_decoration_draw_background(HDC hdc, RECT client, MapLayout layout,
                                             const RenderSnapshot *snapshot) {
    RECT viewport = get_map_viewport_rect(client);
    if (!snapshot || !snapshot->world_generated) return;
    ensure_items(snapshot, viewport, layout); draw_exterior_cache(hdc, client, layout, snapshot);
}

void render_ocean_decoration_draw_overlay(HDC hdc, RECT client, MapLayout layout,
                                          const RenderSnapshot *snapshot) {
    RECT viewport = get_map_viewport_rect(client);
    if (!snapshot || !snapshot->world_generated) return;
    ensure_items(snapshot, viewport, layout); draw_interior_cache(hdc, client, layout, snapshot);
}

void render_ocean_decoration_draw(HDC hdc, RECT client, MapLayout layout,
                                  const RenderSnapshot *snapshot) {
    render_ocean_decoration_draw_background(hdc, client, layout, snapshot); render_ocean_decoration_draw_overlay(hdc, client, layout, snapshot);
}

void render_ocean_decoration_reset_debug(void) {
    item_key = 0; exterior_cache.valid = 0; interior_cache.valid = 0;
    exterior_count = interior_count = 0; item_hash = motif_mask = 0;
    interior_water_only = 1; interior_deep_only = 1;
    interior_shallow_allowed_seen = 0; same_type_spacing_ok = 1;
    interior_min_clearance = 99; motif_overlap_count = 0;
    exterior_texture_score = interior_texture_score = 0;
    exterior_rebuilds = interior_rebuilds = item_rebuilds = 0;
}

OceanDecorationProbeInfo render_ocean_decoration_probe_info(void) {
    OceanDecorationProbeInfo info;
    info.exterior_items = exterior_count; info.interior_items = interior_count;
    info.exterior_rebuilds = exterior_rebuilds; info.interior_rebuilds = interior_rebuilds;
    info.item_rebuilds = item_rebuilds; info.compass_items = 0;
    info.interior_water_only = interior_water_only; info.interior_deep_only = interior_deep_only;
    info.interior_shallow_allowed_seen = interior_shallow_allowed_seen;
    info.same_type_spacing_ok = same_type_spacing_ok;
    info.interior_min_clearance = interior_min_clearance;
    info.motif_overlap_count = motif_overlap_count;
    info.exterior_texture_score = exterior_texture_score;
    info.interior_texture_score = interior_texture_score;
    info.texture_asset_ready = ocean_assets_texture_ready();
    info.motif_asset_ready = ocean_assets_motifs_ready();
    info.primitive_wave_stamps = 0;
    info.item_hash = item_hash;
    info.motif_mask = motif_mask;
    return info;
}
