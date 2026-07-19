#include "game/game_presentation_coast_protection_probe.h"

#include "render/render_water_coast_presentation.h"
#include "render/render_water_coast_smoothing.h"
#include "world/terrain_query.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint64_t mix_hash(uint64_t hash, uint32_t value) {
    hash ^= value;
    return hash * UINT64_C(1099511628211);
}

static uint64_t semantic_hash(const RenderSnapshot *snapshot) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int i;
    hash = mix_hash(hash, (uint32_t)snapshot->map_w);
    hash = mix_hash(hash, (uint32_t)snapshot->map_h);
    for (i = 0; i < snapshot->map_w * snapshot->map_h; i++) {
        const SnapshotTile *tile = &snapshot->tiles[i];
        hash = mix_hash(hash, (uint32_t)tile->geography |
            ((uint32_t)tile->water_depth << 8) |
            ((uint32_t)tile->river << 16));
    }
    for (i = 0; snapshot->rivers.paths &&
                i < snapshot->rivers.path_count; i++) {
        const SnapshotRiverPath *path = &snapshot->rivers.paths[i];
        int p;
        hash = mix_hash(hash, path->semantic_flags |
            ((uint32_t)path->end_flags << 8) |
            ((uint32_t)path->point_count << 16));
        for (p = 0; p < path->point_count; p++)
            hash = mix_hash(hash, path->points[p].x |
                ((uint32_t)path->points[p].y << 12) |
                ((uint32_t)path->points[p].semantic_flags << 24));
    }
    for (i = 0; i < snapshot->region_count; i++) {
        const SnapshotRegion *region = &snapshot->regions[i];
        hash = mix_hash(hash, (uint32_t)region->has_port_site |
            ((uint32_t)region->port_x << 8) |
            ((uint32_t)region->port_y << 20));
    }
    for (i = 0; i < snapshot->city_count; i++) {
        const SnapshotCity *city = &snapshot->cities[i];
        hash = mix_hash(hash, (uint32_t)city->port |
            ((uint32_t)city->port_x << 8) |
            ((uint32_t)city->port_y << 20));
    }
    for (i = 0; i < snapshot->lane_count; i++) {
        const SnapshotSeaLane *lane = &snapshot->lanes[i];
        hash = mix_hash(hash, (uint32_t)lane->active |
            ((uint32_t)lane->from_sea_entry.x << 8) |
            ((uint32_t)lane->from_sea_entry.y << 20));
        hash = mix_hash(hash, (uint32_t)lane->to_sea_entry.x |
            ((uint32_t)lane->to_sea_entry.y << 12));
    }
    return hash;
}

static int component_filter_contract(FILE *summary) {
    enum { W = 32, H = 20 };
    unsigned char ocean[W * H] = {0};
    unsigned char protected_ocean[W * H] = {0};
    unsigned char suppressed[W * H] = {0};
    uint64_t components = 0, tiles = 0, bytes = 0;
    int x;
    int ok;
    for (x = 2; x <= 12; x++) {
        suppressed[4 * W + x] = 1;
        suppressed[10 * W + x] = 1;
        suppressed[16 * W + x] = 1;
    }
    suppressed[5 * W + 13] = 1;
    suppressed[6 * W + 14] = 1;
    suppressed[7 * W + 15] = 1;
    protected_ocean[4 * W + 7] = 1;
    for (x = 2; x <= 12; x++) suppressed[x * W + 24] = 1;
    for (x = 4; x <= 10; x += 2) suppressed[x * W + 25] = 1;
    protected_ocean[2 * W + 24] = 1;
    suppressed[3 * W + 30] = protected_ocean[3 * W + 30] = 1;
    memcpy(ocean, suppressed, sizeof(ocean));
    ocean[4 * W + 1] = 1;
    ok = render_water_coast_presentation_preserve_anchored_components(
        ocean, protected_ocean, W, H, suppressed,
        &components, &tiles, &bytes);
    for (x = 2; ok && x <= 12; x++)
        ok &= suppressed[4 * W + x] == (unsigned char)(x > 7) &&
              suppressed[10 * W + x] == 1 &&
              suppressed[16 * W + x] == 1;
    ok &= suppressed[5 * W + 13] == 1 &&
          suppressed[6 * W + 14] == 1 &&
          suppressed[7 * W + 15] == 1;
    for (x = 2; ok && x <= 12; x++)
        ok &= suppressed[x * W + 24] == 1;
    for (x = 4; ok && x <= 10; x += 2)
        ok &= suppressed[x * W + 25] == 1;
    ok &= suppressed[3 * W + 30] == 1;
    ok &= components == 1 && tiles == 6 &&
          bytes == W * H * 3u * sizeof(int);
    fprintf(summary,
            "case=coast_protection_component ok=%d components=%llu "
            "tiles=%llu disconnected=1 transient=%llu\n", ok,
            (unsigned long long)components, (unsigned long long)tiles,
            (unsigned long long)bytes);
    return ok;
}

static void set_ocean(RenderSnapshot *snapshot, int x, int y,
                      int geography, int depth) {
    SnapshotTile *tile = &snapshot->tiles[y * snapshot->map_w + x];
    tile->geography = (unsigned char)geography;
    tile->water_depth = (unsigned char)depth;
}

static RenderSnapshot *build_synthetic_fixture(void) {
    RenderSnapshot *snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    int x, y;
    if (!snapshot) return NULL;
    snapshot->world_generated = 1;
    snapshot->map_w = 48;
    snapshot->map_h = 32;
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++)
            snapshot->tiles[y * snapshot->map_w + x].geography = GEO_PLAIN;
        for (x = 0; x <= 8; x++)
            set_ocean(snapshot, x, y, GEO_OCEAN,
                      x <= 4 ? WATER_DEPTH_DEEP : WATER_DEPTH_SHALLOW);
    }
    for (x = 9; x <= 15; x++) {
        set_ocean(snapshot, x, 18, GEO_BAY, WATER_DEPTH_SHALLOW);
        set_ocean(snapshot, x, 21, GEO_BAY, WATER_DEPTH_SHALLOW);
        set_ocean(snapshot, x, 24, GEO_BAY, WATER_DEPTH_SHALLOW);
    }
    for (y = 4; y <= 6; y++)
        for (x = 30; x <= 32; x++)
            snapshot->tiles[y * snapshot->map_w + x].geography = GEO_LAKE;
    snapshot->region_count = 1;
    snapshot->regions[0].alive = 1;
    snapshot->regions[0].center_x = 20;
    snapshot->regions[0].center_y = 24;
    snapshot->regions[0].capital_x = 20;
    snapshot->regions[0].capital_y = 24;
    snapshot->regions[0].has_port_site = 1;
    snapshot->regions[0].port_x = 19;
    snapshot->regions[0].port_y = 24;
    snapshot->city_count = 1;
    snapshot->cities[0].alive = 1;
    snapshot->cities[0].x = 20;
    snapshot->cities[0].y = 24;
    snapshot->cities[0].port = 1;
    snapshot->cities[0].port_x = 19;
    snapshot->cities[0].port_y = 24;
    snapshot->lane_count = 1;
    snapshot->lanes[0].active = 1;
    snapshot->lanes[0].from_port = (MapPoint){19, 24};
    snapshot->lanes[0].to_port = (MapPoint){19, 24};
    snapshot->lanes[0].from_sea_entry = (MapPoint){15, 24};
    snapshot->lanes[0].to_sea_entry = (MapPoint){15, 24};
    snapshot->lanes[0].point_count = 2;
    snapshot->lanes[0].points[0] = (MapPoint){15, 24};
    snapshot->lanes[0].points[1] = (MapPoint){15, 24};
    snapshot->rivers.valid = 1;
    snapshot->rivers.path_count = 1;
    snapshot->rivers.capacity = 1;
    snapshot->rivers.paths =
        (SnapshotRiverPath *)calloc(1, sizeof(SnapshotRiverPath));
    if (!snapshot->rivers.paths) {
        free(snapshot);
        return NULL;
    }
    snapshot->rivers.paths[0].point_count = 1;
    snapshot->rivers.paths[0].end_flags = SNAPSHOT_RIVER_MOUTH;
    snapshot->rivers.paths[0].points[0].x = 15;
    snapshot->rivers.paths[0].points[0].y = 18;
    snapshot->rivers.paths[0].points[0].semantic_flags =
        SNAPSHOT_RIVER_MOUTH;
    return snapshot;
}

static int row_land_count(const unsigned char *categories, int width,
                          int y, int x0, int x1) {
    int count = 0;
    int x;
    for (x = x0; x <= x1; x++)
        count += categories[y * width + x] == WATER_COAST_PRESENTATION_LAND;
    return count;
}

static int synthetic_builder_contract(FILE *summary) {
    RenderSnapshot *snapshot = build_synthetic_fixture();
    RenderWaterCoastPresentationMetrics first = {0}, second = {0};
    unsigned char *a = NULL;
    unsigned char *b = NULL;
    size_t count;
    uint64_t before = 0, after = 0;
    int mouth_land = 0, port_land = 0, plain_land = 0;
    int lake_failures = 0;
    int sea_x = -1, sea_y = -1;
    int dynamic_alignment = 0;
    int dynamic_independent = 0;
    int stable = 0;
    int x, y;
    int ok = 0;
    if (!snapshot) goto cleanup;
    count = (size_t)snapshot->map_w * (size_t)snapshot->map_h;
    a = (unsigned char *)malloc(count);
    b = (unsigned char *)malloc(count);
    if (!a || !b) goto cleanup;
    before = semantic_hash(snapshot);
    if (!render_water_coast_presentation_build(snapshot, a, &first) ||
        !render_water_coast_presentation_build(snapshot, b, &second))
        goto cleanup;
    after = semantic_hash(snapshot);
    stable = first.presentation_hash == second.presentation_hash &&
             memcmp(a, b, count) == 0;
    mouth_land = row_land_count(a, snapshot->map_w, 18, 9, 15);
    plain_land = row_land_count(a, snapshot->map_w, 21, 9, 15);
    port_land = row_land_count(a, snapshot->map_w, 24, 9, 15);
    for (y = 4; y <= 6; y++)
        for (x = 30; x <= 32; x++)
            lake_failures += a[y * snapshot->map_w + x] !=
                             WATER_COAST_PRESENTATION_LAKE;
    dynamic_alignment =
        a[snapshot->cities[0].port_y * snapshot->map_w +
          snapshot->cities[0].port_x] == WATER_COAST_PRESENTATION_LAND &&
        a[snapshot->lanes[0].from_port.y * snapshot->map_w +
          snapshot->lanes[0].from_port.x] == WATER_COAST_PRESENTATION_LAND &&
        a[snapshot->lanes[0].from_sea_entry.y * snapshot->map_w +
          snapshot->lanes[0].from_sea_entry.x] ==
            WATER_COAST_PRESENTATION_OCEAN;
    snapshot->cities[0].alive = 0;
    snapshot->cities[0].port_x = 37;
    snapshot->cities[0].port_y = 3;
    snapshot->lanes[0].active = 0;
    snapshot->lanes[0].from_sea_entry = (MapPoint){37, 3};
    if (render_water_coast_presentation_build(snapshot, b, &second))
        dynamic_independent = first.presentation_hash ==
                              second.presentation_hash &&
                              memcmp(a, b, count) == 0;
    ok = render_water_coast_presentation_find_shallow_entry(
        snapshot, 19, 24, &sea_x, &sea_y) &&
        sea_x == 15 && sea_y == 24 && mouth_land == 0 && port_land == 0 &&
        plain_land == 0 && lake_failures == 0 &&
        a[24 * snapshot->map_w + 19] == WATER_COAST_PRESENTATION_LAND &&
        a[24 * snapshot->map_w + 20] == WATER_COAST_PRESENTATION_LAND &&
        first.initial_protected_marine_components == 0 &&
        first.initial_protected_marine_tiles == 0 &&
        first.removed_ocean_tiles == 0 && first.filled_land_tiles == 0 &&
        first.ocean_cleanup_tiles == 0 &&
        dynamic_alignment &&
        dynamic_independent && stable &&
        before == after &&
        first.presentation_hash == second.presentation_hash;
    fprintf(summary,
            "case=coast_protection_synthetic ok=%d mouth_land=%d "
            "unanchored_land=%d port_land=%d port_entry=%d,%d "
            "dynamic_alignment=%d dynamic_independent=%d "
            "marine=%llu/%llu removed=%llu cleanup=%llu/%llu/%llu "
            "lakes=%d semantics=%llu/%llu stable=%d\n",
            ok, mouth_land, plain_land, port_land, sea_x, sea_y,
            dynamic_alignment, dynamic_independent,
            (unsigned long long)first.initial_protected_marine_components,
            (unsigned long long)first.initial_protected_marine_tiles,
            (unsigned long long)first.removed_ocean_tiles,
            (unsigned long long)first.ocean_cleanup_semantic_candidates,
            (unsigned long long)first.ocean_cleanup_filled_candidates,
            (unsigned long long)first.ocean_cleanup_tiles, lake_failures,
            (unsigned long long)before, (unsigned long long)after,
            stable);
cleanup:
    free(b);
    free(a);
    if (snapshot) {
        free(snapshot->rivers.paths);
        free(snapshot);
    }
    return ok;
}

static RenderSnapshot *build_cleanup_fixture(void) {
    RenderSnapshot *snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    static const int rows[4] = {7, 10, 13, 16};
    int x, y, r;
    if (!snapshot) return NULL;
    snapshot->world_generated = 1;
    snapshot->map_w = 48;
    snapshot->map_h = 32;
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++)
            snapshot->tiles[y * snapshot->map_w + x].geography = GEO_PLAIN;
        for (x = 0; x <= 8; x++)
            set_ocean(snapshot, x, y, GEO_OCEAN,
                      x <= 4 ? WATER_DEPTH_DEEP : WATER_DEPTH_SHALLOW);
    }
    for (r = 0; r < 4; r++)
        for (x = 9; x <= 15; x++)
            set_ocean(snapshot, x, rows[r], GEO_BAY, WATER_DEPTH_SHALLOW);
    for (y = 24; y <= 26; y++)
        for (x = 30; x <= 32; x++)
            set_ocean(snapshot, x, y, GEO_BAY, WATER_DEPTH_SHALLOW);
    return snapshot;
}

static int cleanup_builder_contract(FILE *summary) {
    RenderSnapshot *snapshot = build_cleanup_fixture();
    RenderWaterCoastPresentationMetrics first = {0}, second = {0};
    unsigned char *a = NULL;
    unsigned char *b = NULL;
    size_t count;
    uint64_t before = 0, after = 0;
    int fringe_land = 0;
    int compact_bay_failures = 0;
    int x, y;
    int ok = 0;
    if (!snapshot) goto cleanup;
    count = (size_t)snapshot->map_w * (size_t)snapshot->map_h;
    a = (unsigned char *)malloc(count);
    b = (unsigned char *)malloc(count);
    if (!a || !b) goto cleanup;
    before = semantic_hash(snapshot);
    if (!render_water_coast_presentation_build(snapshot, a, &first) ||
        !render_water_coast_presentation_build(snapshot, b, &second))
        goto cleanup;
    after = semantic_hash(snapshot);
    for (y = 7; y <= 16; y++)
        for (x = 9; x <= 15; x++)
            fringe_land += a[y * snapshot->map_w + x] ==
                           WATER_COAST_PRESENTATION_LAND;
    for (y = 24; y <= 26; y++)
        for (x = 30; x <= 32; x++)
            compact_bay_failures += a[y * snapshot->map_w + x] !=
                                    WATER_COAST_PRESENTATION_OCEAN;
    ok = fringe_land == 42 && compact_bay_failures == 0 &&
          first.ocean_cleanup_semantic_candidates == 0 &&
         first.ocean_cleanup_filled_candidates == 0 &&
         first.ocean_cleanup_tiles == 0 &&
         first.removed_ocean_tiles == 0 &&
         first.filled_land_tiles == 0 && before == after &&
         first.presentation_hash == second.presentation_hash &&
         memcmp(a, b, count) == 0;
    fprintf(summary,
            "case=coast_source_authority ok=%d fringe_land=%d/42 "
            "compact_bay_failures=%d cleanup=%llu/%llu/%llu "
            "removed=%llu final_filled=%llu semantics=%llu/%llu stable=%d\n",
            ok, fringe_land, compact_bay_failures,
            (unsigned long long)first.ocean_cleanup_semantic_candidates,
            (unsigned long long)first.ocean_cleanup_filled_candidates,
            (unsigned long long)first.ocean_cleanup_tiles,
            (unsigned long long)first.removed_ocean_tiles,
            (unsigned long long)first.filled_land_tiles,
            (unsigned long long)before, (unsigned long long)after,
            memcmp(a, b, count) == 0);
cleanup:
    free(b);
    free(a);
    free(snapshot);
    return ok;
}

static int category_at(const RenderSnapshot *snapshot,
                       const unsigned char *categories, int x, int y,
                       int expected) {
    return x >= 0 && y >= 0 && x < snapshot->map_w && y < snapshot->map_h &&
           categories[y * snapshot->map_w + x] == expected;
}

static int natural_contract(FILE *summary, const RenderSnapshot *snapshot) {
    RenderWaterCoastPresentationMetrics coast = {0};
    size_t count = (size_t)snapshot->map_w * (size_t)snapshot->map_h;
    unsigned char *categories = (unsigned char *)malloc(count);
    uint64_t before, after;
    int port_samples = 0, port_failures = 0;
    int city_samples = 0, city_failures = 0;
    int lane_samples = 0, lane_failures = 0;
    int lake_samples = 0, lake_failures = 0;
    int category_failures = 0;
    int removed_ocean = 0;
    int i;
    int ok = 0;
    if (!categories) goto cleanup;
    before = semantic_hash(snapshot);
    if (!render_water_coast_presentation_build(snapshot, categories, &coast))
        goto cleanup;
    for (i = 0; i < (int)count; i++) {
        int geography = snapshot->tiles[i].geography;
        int semantic_ocean = geography == GEO_OCEAN || geography == GEO_BAY;
        if (geography == GEO_LAKE)
            category_failures += categories[i] !=
                                 WATER_COAST_PRESENTATION_LAKE;
        else if (semantic_ocean) {
            category_failures += categories[i] !=
                                 WATER_COAST_PRESENTATION_OCEAN;
            removed_ocean +=
                categories[i] == WATER_COAST_PRESENTATION_LAND;
        } else
            category_failures +=
                categories[i] != WATER_COAST_PRESENTATION_LAND;
        if (geography == GEO_LAKE) {
            lake_samples++;
            lake_failures += categories[i] != WATER_COAST_PRESENTATION_LAKE;
        }
    }
    for (i = 0; i < snapshot->region_count; i++) {
        const SnapshotRegion *region = &snapshot->regions[i];
        int sea_x, sea_y;
        if (!region->alive || !region->has_port_site) continue;
        port_samples++;
        port_failures += !category_at(snapshot, categories,
            region->port_x, region->port_y, WATER_COAST_PRESENTATION_LAND);
        if (!render_water_coast_presentation_find_shallow_entry(
                snapshot, region->port_x, region->port_y, &sea_x, &sea_y)) {
            port_failures++;
        } else {
            port_failures += !category_at(snapshot, categories, sea_x, sea_y,
                                          WATER_COAST_PRESENTATION_OCEAN);
        }
    }
    for (i = 0; i < snapshot->city_count; i++) {
        const SnapshotCity *city = &snapshot->cities[i];
        if (!city->alive || !city->port) continue;
        city_samples++;
        city_failures += !category_at(snapshot, categories,
            city->port_x, city->port_y, WATER_COAST_PRESENTATION_LAND);
    }
    for (i = 0; i < snapshot->lane_count; i++) {
        const SnapshotSeaLane *lane = &snapshot->lanes[i];
        if (!lane->active) continue;
        lane_samples += 4;
        lane_failures += !category_at(snapshot, categories,
            lane->from_port.x, lane->from_port.y,
            WATER_COAST_PRESENTATION_LAND);
        lane_failures += !category_at(snapshot, categories,
            lane->to_port.x, lane->to_port.y,
            WATER_COAST_PRESENTATION_LAND);
        lane_failures += !category_at(snapshot, categories,
            lane->from_sea_entry.x, lane->from_sea_entry.y,
            WATER_COAST_PRESENTATION_OCEAN);
        lane_failures += !category_at(snapshot, categories,
            lane->to_sea_entry.x, lane->to_sea_entry.y,
            WATER_COAST_PRESENTATION_OCEAN);
    }
    after = semantic_hash(snapshot);
    ok = category_failures == 0 && port_failures == 0 &&
         city_failures == 0 && lane_failures == 0 &&
         lake_failures == 0 && before == after &&
         coast.removed_ocean_tiles == 0 && removed_ocean == 0 &&
         coast.filled_land_tiles == 0 &&
         coast.ocean_cleanup_tiles == 0;
    fprintf(summary,
            "case=coast_protection_natural ok=%d category_failures=%d "
            "removed_ocean=%d "
            "ports=%d/%d cities=%d/%d lanes=%d/%d lakes=%d/%d "
            "removed=%llu filled=%llu cleanup=%llu semantics=%llu/%llu\n",
            ok, category_failures, removed_ocean,
            port_samples - port_failures, port_samples,
            city_samples - city_failures, city_samples,
            lane_samples - lane_failures, lane_samples,
            lake_samples - lake_failures, lake_samples,
            (unsigned long long)coast.removed_ocean_tiles,
            (unsigned long long)coast.filled_land_tiles,
            (unsigned long long)coast.ocean_cleanup_tiles,
            (unsigned long long)before, (unsigned long long)after);
cleanup:
    free(categories);
    return ok;
}

int game_presentation_coast_protection_synthetic_probe(FILE *summary) {
    int component_ok;
    int synthetic_ok;
    int cleanup_ok;
    if (!summary) return 0;
    component_ok = component_filter_contract(summary);
    synthetic_ok = synthetic_builder_contract(summary);
    cleanup_ok = cleanup_builder_contract(summary);
    return component_ok && synthetic_ok && cleanup_ok;
}

int game_presentation_coast_protection_probe(
    FILE *summary, const RenderSnapshot *natural_snapshot) {
    int synthetic_ok;
    int natural_ok;
    if (!summary || !natural_snapshot || !natural_snapshot->world_generated)
        return 0;
    synthetic_ok = game_presentation_coast_protection_synthetic_probe(summary);
    natural_ok = natural_contract(summary, natural_snapshot);
    fprintf(summary, "case=coast_protection_overall ok=%d\n",
            synthetic_ok && natural_ok);
    return synthetic_ok && natural_ok;
}
