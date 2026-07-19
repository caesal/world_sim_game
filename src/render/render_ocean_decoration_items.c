#include "render/render_ocean_decoration_items.h"

#include "render/render_common.h"
#include "render/render_ocean_assets.h"
#include "render/render_ocean_decoration_rules.h"
#include "render/render_ocean_decoration_water.h"

#include <stdlib.h>

enum {
    OCEAN_EXT_MAX = 64,
    OCEAN_INT_MAX = 20,
    OCEAN_EXT_TARGET = 24,
    OCEAN_INT_TARGET = 10,
    OCEAN_NORM = 10000,
    OCEAN_NOMINAL_W = 1100,
    OCEAN_NOMINAL_H = 620
};

static OceanDecorationItem exterior_items[OCEAN_EXT_MAX];
static OceanDecorationItem interior_items[OCEAN_INT_MAX];
static OceanDecorationItemSet item_set;
static int source_valid;
static int source_map_w, source_map_h, source_terrain_revision;
static int source_region_count, source_asset_count;

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
        if (!info || (exterior ? !info->allow_exterior : !info->allow_interior)) continue;
        total += max(1, info->weight);
    }
    if (total <= 0) return -1;
    pick = (int)(next_rand(rng) % (unsigned int)total);
    for (i = 0; i < count; i++) {
        const OceanMotifAssetInfo *info = ocean_assets_motif_info(i);
        int weight;
        if (!info || (exterior ? !info->allow_exterior : !info->allow_interior)) continue;
        weight = max(1, info->weight);
        if (pick < weight) return i;
        pick -= weight;
    }
    return -1;
}

static int footprint_tiles(const RenderSnapshot *snapshot, unsigned char type,
                           int size, int vertical) {
    const OceanMotifAssetInfo *info = ocean_assets_motif_info(type);
    int scaled = ocean_decoration_motif_scaled_footprint(info, size, vertical);
    int span = snapshot ? (vertical ? snapshot->map_h : snapshot->map_w) :
               (vertical ? 64 : 96);
    int denom = vertical ? 540 : 900;
    return max(5, (scaled * span + denom - 1) / denom + 2);
}

int render_ocean_decoration_item_clearance_tiles(
    const RenderSnapshot *snapshot, const OceanDecorationItem *item) {
    const OceanMotifAssetInfo *info;
    int w, h, coast, radius;
    if (!item) return 0;
    info = ocean_assets_motif_info(item->type);
    w = footprint_tiles(snapshot, item->type, item->size, 0);
    h = footprint_tiles(snapshot, item->type, item->size, 1);
    coast = info ? info->min_coast_clear_tiles : 6;
    radius = (max(w, h) + 1) / 2 + (max(w, h) >= 12 ? 4 : 3);
    return max(coast, radius);
}

static int scaled_width(unsigned char type, unsigned int *rng) {
    const OceanMotifAssetInfo *info = ocean_assets_motif_info(type);
    int base = ocean_decoration_motif_base_width(info);
    return rand_range(rng, base * 85 / 100, base * 115 / 100);
}

static int collides_exterior(const OceanDecorationItem *items, int count,
                             OceanDecorationItem item) {
    const OceanMotifAssetInfo *info = ocean_assets_motif_info(item.type);
    int w = ocean_decoration_motif_scaled_footprint(info, item.size, 0) * OCEAN_NORM /
            OCEAN_NOMINAL_W * 13 / 10 + 60;
    int h = ocean_decoration_motif_scaled_footprint(info, item.size, 1) * OCEAN_NORM /
            OCEAN_NOMINAL_H * 13 / 10 + 60;
    int i;
    for (i = 0; i < count; i++) {
        const OceanMotifAssetInfo *other = ocean_assets_motif_info(items[i].type);
        int margin = ocean_decoration_exterior_spacing_norm(item.type == items[i].type);
        int ow = ocean_decoration_motif_scaled_footprint(other, items[i].size, 0) *
                 OCEAN_NORM / OCEAN_NOMINAL_W * 13 / 10 + 60;
        int oh = ocean_decoration_motif_scaled_footprint(other, items[i].size, 1) *
                 OCEAN_NORM / OCEAN_NOMINAL_H * 13 / 10 + 60;
        if (abs(item.x - items[i].x) * 2 < w + ow + margin &&
            abs(item.y - items[i].y) * 2 < h + oh + margin) return 1;
    }
    return 0;
}

static int collides_interior(const RenderSnapshot *snapshot,
                             const OceanDecorationItem *items, int count,
                             OceanDecorationItem item) {
    int w = footprint_tiles(snapshot, item.type, item.size, 0);
    int h = footprint_tiles(snapshot, item.type, item.size, 1);
    int i;
    for (i = 0; i < count; i++) {
        int margin = ocean_decoration_interior_spacing_tiles(item.type == items[i].type);
        int ow = footprint_tiles(snapshot, items[i].type, items[i].size, 0);
        int oh = footprint_tiles(snapshot, items[i].type, items[i].size, 1);
        if (abs(item.x - items[i].x) * 2 < w + ow + margin &&
            abs(item.y - items[i].y) * 2 < h + oh + margin) return 1;
    }
    return 0;
}

static void add_item(OceanDecorationItem *items, int *count, int max_count,
                     OceanDecorationItem item) {
    if (*count >= max_count) return;
    items[(*count)++] = item;
    item_set.motif_mask |= 1u << item.type;
    item_set.hash = mix_u32(item_set.hash, (unsigned int)(item.type + item.x * 17 +
                            item.y * 131 + item.size * 8191 + item.variant));
}

static void validate_spacing(const RenderSnapshot *snapshot) {
    int i, j;
    item_set.same_type_spacing_ok = item_set.exterior_spacing_ok = 1;
    item_set.motif_overlap_count = item_set.motif_spacing_violation_count = 0;
    for (i = 0; i < item_set.exterior_count; i++) {
        for (j = i + 1; j < item_set.exterior_count; j++) {
            const OceanDecorationItem *a = &exterior_items[i];
            const OceanDecorationItem *b = &exterior_items[j];
            const OceanMotifAssetInfo *ai = ocean_assets_motif_info(a->type);
            const OceanMotifAssetInfo *bi = ocean_assets_motif_info(b->type);
            int aw = ocean_decoration_motif_scaled_footprint(ai, a->size, 0) * OCEAN_NORM / OCEAN_NOMINAL_W * 13 / 10 + 60;
            int ah = ocean_decoration_motif_scaled_footprint(ai, a->size, 1) * OCEAN_NORM / OCEAN_NOMINAL_H * 13 / 10 + 60;
            int bw = ocean_decoration_motif_scaled_footprint(bi, b->size, 0) * OCEAN_NORM / OCEAN_NOMINAL_W * 13 / 10 + 60;
            int bh = ocean_decoration_motif_scaled_footprint(bi, b->size, 1) * OCEAN_NORM / OCEAN_NOMINAL_H * 13 / 10 + 60;
            int margin = ocean_decoration_exterior_spacing_norm(a->type == b->type);
            int cx = abs(a->x - b->x) * 2, cy = abs(a->y - b->y) * 2;
            if (cx < aw + bw && cy < ah + bh) item_set.motif_overlap_count++;
            if (cx < aw + bw + margin && cy < ah + bh + margin) {
                item_set.motif_spacing_violation_count++;
                item_set.exterior_spacing_ok = 0;
                if (a->type == b->type) item_set.same_type_spacing_ok = 0;
            }
        }
    }
    for (i = 0; i < item_set.interior_count; i++) {
        for (j = i + 1; j < item_set.interior_count; j++) {
            const OceanDecorationItem *a = &interior_items[i];
            const OceanDecorationItem *b = &interior_items[j];
            int aw = footprint_tiles(snapshot, a->type, a->size, 0);
            int ah = footprint_tiles(snapshot, a->type, a->size, 1);
            int bw = footprint_tiles(snapshot, b->type, b->size, 0);
            int bh = footprint_tiles(snapshot, b->type, b->size, 1);
            int margin = ocean_decoration_interior_spacing_tiles(a->type == b->type);
            if (abs(a->x - b->x) * 2 < aw + bw && abs(a->y - b->y) * 2 < ah + bh)
                item_set.motif_overlap_count++;
            if (abs(a->x - b->x) * 2 < aw + bw + margin &&
                abs(a->y - b->y) * 2 < ah + bh + margin) {
                item_set.motif_spacing_violation_count++;
                if (a->type == b->type) item_set.same_type_spacing_ok = 0;
            }
        }
    }
}

static void rebuild(const RenderSnapshot *snapshot) {
    unsigned int rng;
    int attempts;
    int assets = ocean_assets_motif_count();
    item_set.key = mix_u32(snapshot_seed(snapshot), (unsigned int)assets);
    rng = item_set.key;
    item_set.exterior_count = item_set.interior_count = 0;
    item_set.hash = 2166136261u;
    item_set.motif_mask = 0;
    for (attempts = 0; attempts < 520 && item_set.exterior_count < OCEAN_EXT_TARGET; attempts++) {
        OceanDecorationItem item;
        int picked = random_motif_asset(&rng, 1);
        if (picked < 0) break;
        item.type = (unsigned char)picked;
        item.variant = (unsigned char)rand_range(&rng, 0, 15);
        item.interior = 0;
        item.opacity = (unsigned char)rand_range(&rng, 42, 58);
        item.x = (short)rand_range(&rng, 650, OCEAN_NORM - 650);
        item.y = (short)rand_range(&rng, 650, OCEAN_NORM - 650);
        item.size = (short)scaled_width(item.type, &rng);
        if (!collides_exterior(exterior_items, item_set.exterior_count, item))
            add_item(exterior_items, &item_set.exterior_count, OCEAN_EXT_MAX, item);
    }
    for (attempts = 0; attempts < 1400 && item_set.interior_count < OCEAN_INT_TARGET; attempts++) {
        OceanDecorationItem item;
        int needed;
        int picked = random_motif_asset(&rng, 0);
        if (picked < 0 || snapshot->map_w < 5 || snapshot->map_h < 5) break;
        item.type = (unsigned char)picked;
        item.variant = (unsigned char)rand_range(&rng, 0, 15);
        item.interior = 1;
        item.opacity = (unsigned char)rand_range(&rng, 22, 34);
        item.x = (short)rand_range(&rng, 2, snapshot->map_w - 3);
        item.y = (short)rand_range(&rng, 2, snapshot->map_h - 3);
        item.size = (short)scaled_width(item.type, &rng);
        needed = render_ocean_decoration_item_clearance_tiles(snapshot, &item);
        if (ocean_decoration_motif_clearance(snapshot, item.x, item.y, item.type, needed) < needed ||
            !far_from_city_and_lane(snapshot, item.x, item.y) ||
            collides_interior(snapshot, interior_items, item_set.interior_count, item)) continue;
        add_item(interior_items, &item_set.interior_count, OCEAN_INT_MAX, item);
    }
    validate_spacing(snapshot);
    source_valid = 1;
    source_map_w = snapshot->map_w;
    source_map_h = snapshot->map_h;
    source_terrain_revision = snapshot->terrain_revision;
    source_region_count = snapshot->region_count;
    source_asset_count = assets;
    item_set.item_rebuilds++;
}

const OceanDecorationItemSet *render_ocean_decoration_items_ensure(
    const RenderSnapshot *snapshot) {
    if (!snapshot) return &item_set;
    if (!source_valid || source_map_w != snapshot->map_w || source_map_h != snapshot->map_h ||
        source_terrain_revision != snapshot->terrain_revision ||
        source_region_count != snapshot->region_count ||
        source_asset_count != ocean_assets_motif_count()) rebuild(snapshot);
    item_set.exterior_items = exterior_items;
    item_set.interior_items = interior_items;
    return &item_set;
}

const OceanDecorationItemSet *render_ocean_decoration_items_ready(
    const RenderSnapshot *snapshot) {
    if (!snapshot || !source_valid || source_map_w != snapshot->map_w ||
        source_map_h != snapshot->map_h ||
        source_terrain_revision != snapshot->terrain_revision ||
        source_region_count != snapshot->region_count ||
        source_asset_count != ocean_assets_motif_count_loaded()) return NULL;
    return &item_set;
}

void render_ocean_decoration_items_reset(void) {
    int rebuilds = item_set.item_rebuilds;
    item_set = (OceanDecorationItemSet){0};
    item_set.same_type_spacing_ok = item_set.exterior_spacing_ok = 1;
    item_set.item_rebuilds = 0;
    (void)rebuilds;
    source_valid = 0;
    source_map_w = source_map_h = source_terrain_revision = 0;
    source_region_count = source_asset_count = 0;
}
