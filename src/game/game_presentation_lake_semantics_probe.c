#include "game/game_presentation_lake_semantics_probe.h"

#include "render/render_water_coverage_raster.h"

#include <stdint.h>
#include <string.h>

enum {
    LAKE_FIXTURE_SCALE = 8,
    LAKE_FIXTURE_SUBSAMPLES = 4,
    LAKE_FIXTURE_MAX_W = 12,
    LAKE_FIXTURE_MAX_H = 8,
    LAKE_FIXTURE_MAX_PIXELS = LAKE_FIXTURE_MAX_W * LAKE_FIXTURE_MAX_H *
                              LAKE_FIXTURE_SCALE * LAKE_FIXTURE_SCALE
};

typedef struct {
    unsigned char ocean[LAKE_FIXTURE_MAX_PIXELS];
    unsigned char lake[LAKE_FIXTURE_MAX_PIXELS];
    unsigned char scratch[LAKE_FIXTURE_MAX_PIXELS];
    RenderWaterCoverageRasterMetrics metrics;
    int width;
    int height;
} LakeRasterResult;

static uint64_t hash_bytes(uint64_t hash, const unsigned char *bytes,
                           int count) {
    int i;
    for (i = 0; i < count; i++) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t raster_hash(const LakeRasterResult *result) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int count = result->width * result->height;
    hash = hash_bytes(hash, result->ocean, count);
    return hash_bytes(hash, result->lake, count);
}

static int rasterize_fixture(const unsigned char *source, int map_w, int map_h,
                             LakeRasterResult *result) {
    int count;
    if (!source || !result || map_w <= 0 || map_h <= 0 ||
        map_w > LAKE_FIXTURE_MAX_W || map_h > LAKE_FIXTURE_MAX_H) return 0;
    memset(result, 0, sizeof(*result));
    result->width = map_w * LAKE_FIXTURE_SCALE;
    result->height = map_h * LAKE_FIXTURE_SCALE;
    count = result->width * result->height;
    return count <= LAKE_FIXTURE_MAX_PIXELS &&
           render_water_coverage_rasterize(
               source, map_w, map_h, LAKE_FIXTURE_SCALE,
               LAKE_FIXTURE_SUBSAMPLES, result->ocean, result->lake,
               result->scratch, &result->metrics);
}

static int results_equal(const LakeRasterResult *left,
                         const LakeRasterResult *right) {
    int count;
    if (left->width != right->width || left->height != right->height)
        return 0;
    count = left->width * left->height;
    return memcmp(left->ocean, right->ocean, (size_t)count) == 0 &&
           memcmp(left->lake, right->lake, (size_t)count) == 0 &&
           memcmp(&left->metrics, &right->metrics,
                  sizeof(left->metrics)) == 0;
}

static int tile_is_clear(const LakeRasterResult *result, int tile_x,
                         int tile_y) {
    int x;
    int y;
    for (y = tile_y * LAKE_FIXTURE_SCALE;
         y < (tile_y + 1) * LAKE_FIXTURE_SCALE; y++) {
        for (x = tile_x * LAKE_FIXTURE_SCALE;
             x < (tile_x + 1) * LAKE_FIXTURE_SCALE; x++) {
            if (result->lake[y * result->width + x] != 0) return 0;
        }
    }
    return 1;
}

static int tile_center_has_lake(const LakeRasterResult *result, int tile_x,
                                int tile_y) {
    int x = tile_x * LAKE_FIXTURE_SCALE + LAKE_FIXTURE_SCALE / 2;
    int y = tile_y * LAKE_FIXTURE_SCALE + LAKE_FIXTURE_SCALE / 2;
    return result->lake[y * result->width + x] != 0;
}

static int every_lake_pixel_supported(const unsigned char *source, int map_w,
                                      const LakeRasterResult *result) {
    int x;
    int y;
    for (y = 0; y < result->height; y++) {
        for (x = 0; x < result->width; x++) {
            if (result->lake[y * result->width + x] == 0) continue;
            if (source[(y / LAKE_FIXTURE_SCALE) * map_w +
                       x / LAKE_FIXTURE_SCALE] != RENDER_WATER_CATEGORY_LAKE)
                return 0;
        }
    }
    return 1;
}

static int positive_lake_components(const LakeRasterResult *result) {
    unsigned char visited[LAKE_FIXTURE_MAX_PIXELS] = {0};
    int queue[LAKE_FIXTURE_MAX_PIXELS];
    int components = 0;
    int start;
    int count = result->width * result->height;
    for (start = 0; start < count; start++) {
        int head = 0;
        int tail = 0;
        if (!result->lake[start] || visited[start]) continue;
        components++;
        visited[start] = 1;
        queue[tail++] = start;
        while (head < tail) {
            int current = queue[head++];
            int x = current % result->width;
            int y = current / result->width;
            int neighbor[4];
            int i;
            neighbor[0] = x > 0 ? current - 1 : -1;
            neighbor[1] = x + 1 < result->width ? current + 1 : -1;
            neighbor[2] = y > 0 ? current - result->width : -1;
            neighbor[3] = y + 1 < result->height ?
                          current + result->width : -1;
            for (i = 0; i < 4; i++) {
                int next = neighbor[i];
                if (next < 0 || visited[next] || !result->lake[next]) continue;
                visited[next] = 1;
                queue[tail++] = next;
            }
        }
    }
    return components;
}

static int nonzero_count(const unsigned char *pixels, int count) {
    int nonzero = 0;
    int i;
    for (i = 0; i < count; i++) nonzero += pixels[i] != 0;
    return nonzero;
}

static int diagonal_fixture(FILE *summary) {
    unsigned char source[4 * 4] = {0};
    LakeRasterResult first;
    LakeRasterResult second;
    int components;
    int deterministic;
    int supported;
    int ok;
    source[1 * 4 + 1] = RENDER_WATER_CATEGORY_LAKE;
    source[2 * 4 + 2] = RENDER_WATER_CATEGORY_LAKE;
    if (!rasterize_fixture(source, 4, 4, &first) ||
        !rasterize_fixture(source, 4, 4, &second)) return 0;
    components = positive_lake_components(&first);
    deterministic = results_equal(&first, &second);
    supported = every_lake_pixel_supported(source, 4, &first);
    ok = components == 2 && deterministic && supported &&
         tile_center_has_lake(&first, 1, 1) &&
         tile_center_has_lake(&first, 2, 2) &&
         first.metrics.lake_diagonal_cells == 1;
    fprintf(summary,
            "case=lake_semantics_diagonal ok=%d components=%d "
            "semantic_support=%d deterministic=%d diagonal_cells=%llu "
            "land_pixels_rejected=%llu hash=%llu\n",
            ok, components, supported, deterministic,
            (unsigned long long)first.metrics.lake_diagonal_cells,
            (unsigned long long)first.metrics.lake_land_pixels_rejected,
            (unsigned long long)raster_hash(&first));
    return ok;
}

static void add_ring(unsigned char *source, int map_w, int center_x,
                     int center_y) {
    int x;
    int y;
    for (y = center_y - 1; y <= center_y + 1; y++) {
        for (x = center_x - 1; x <= center_x + 1; x++) {
            if (x != center_x || y != center_y)
                source[y * map_w + x] = RENDER_WATER_CATEGORY_LAKE;
        }
    }
}

static int land_hole_fixture(FILE *summary) {
    unsigned char source[11 * 7] = {0};
    LakeRasterResult first;
    LakeRasterResult second;
    int deterministic;
    int supported;
    int hole_clear;
    int city_clear;
    int ok;
    add_ring(source, 11, 3, 3);
    add_ring(source, 11, 8, 3);
    if (!rasterize_fixture(source, 11, 7, &first) ||
        !rasterize_fixture(source, 11, 7, &second)) return 0;
    deterministic = results_equal(&first, &second);
    supported = every_lake_pixel_supported(source, 11, &first);
    hole_clear = tile_is_clear(&first, 3, 3);
    city_clear = tile_is_clear(&first, 8, 3);
    ok = deterministic && supported && hole_clear && city_clear;
    fprintf(summary,
            "case=lake_semantics_land_holes ok=%d ring_center_clear=%d "
            "city_land_tile_clear=%d semantic_support=%d deterministic=%d "
            "land_pixels_rejected=%llu hash=%llu\n",
            ok, hole_clear, city_clear, supported, deterministic,
            (unsigned long long)first.metrics.lake_land_pixels_rejected,
            (unsigned long long)raster_hash(&first));
    return ok;
}

static int separated_ocean_fixture(FILE *summary) {
    unsigned char baseline_source[12 * 8] = {0};
    unsigned char combined_source[12 * 8] = {0};
    LakeRasterResult baseline;
    LakeRasterResult combined;
    LakeRasterResult repeated;
    int x;
    int y;
    int pixel_count;
    int ocean_equal;
    int deterministic;
    int supported;
    int lake_pixels;
    int ok;
    for (y = 0; y < 8; y++) {
        for (x = 0; x <= 2; x++) {
            baseline_source[y * 12 + x] = RENDER_WATER_CATEGORY_OCEAN;
        }
    }
    memcpy(combined_source, baseline_source, sizeof(combined_source));
    for (y = 2; y <= 5; y++) {
        for (x = 8; x <= 9; x++) {
            combined_source[y * 12 + x] = RENDER_WATER_CATEGORY_LAKE;
        }
    }
    if (!rasterize_fixture(baseline_source, 12, 8, &baseline) ||
        !rasterize_fixture(combined_source, 12, 8, &combined) ||
        !rasterize_fixture(combined_source, 12, 8, &repeated)) return 0;
    pixel_count = baseline.width * baseline.height;
    ocean_equal = memcmp(baseline.ocean, combined.ocean,
                         (size_t)pixel_count) == 0;
    deterministic = results_equal(&combined, &repeated);
    supported = every_lake_pixel_supported(combined_source, 12, &combined);
    lake_pixels = nonzero_count(combined.lake, pixel_count);
    ok = ocean_equal && deterministic && supported && lake_pixels > 0;
    fprintf(summary,
            "case=lake_semantics_separated_ocean ok=%d ocean_unchanged=%d "
            "lake_pixels=%d semantic_support=%d deterministic=%d "
            "samples=%llu overlap_removed=%llu ocean_hash=%llu\n",
            ok, ocean_equal, lake_pixels, supported, deterministic,
            (unsigned long long)combined.metrics.raster_samples,
            (unsigned long long)combined.metrics.ocean_overlap_pixels_removed,
            (unsigned long long)hash_bytes(
                UINT64_C(1469598103934665603), combined.ocean, pixel_count));
    return ok;
}

static int adjacent_category_contract(
    const unsigned char *source, int map_w,
    const LakeRasterResult *ocean_baseline,
    const LakeRasterResult *combined,
    int *ocean_unchanged, int *cross_category_clear,
    int *shared_boundary_visible) {
    int x;
    int y;
    int ocean_pixels = 0;
    int lake_pixels = 0;
    *ocean_unchanged = 1;
    *cross_category_clear = 1;
    *shared_boundary_visible = 1;
    for (y = 0; y < combined->height; y++) {
        for (x = 0; x < combined->width; x++) {
            int index = y * combined->width + x;
            int owner = source[(y / LAKE_FIXTURE_SCALE) * map_w +
                               x / LAKE_FIXTURE_SCALE];
            unsigned char ocean = combined->ocean[index];
            unsigned char lake = combined->lake[index];
            if (ocean && lake) *cross_category_clear = 0;
            if (owner == RENDER_WATER_CATEGORY_OCEAN) {
                ocean_pixels++;
                if (ocean != ocean_baseline->ocean[index] || lake)
                    *ocean_unchanged = 0;
            } else if (owner == RENDER_WATER_CATEGORY_LAKE) {
                lake_pixels++;
                if (ocean) *cross_category_clear = 0;
            } else if (lake) {
                *cross_category_clear = 0;
            }
        }
    }
    for (y = 1; y <= 4; y++) {
        int left_center = (y * LAKE_FIXTURE_SCALE +
                           LAKE_FIXTURE_SCALE / 2) * combined->width;
        int left_index = left_center +
                         4 * LAKE_FIXTURE_SCALE + LAKE_FIXTURE_SCALE / 2;
        int right_index = left_center +
                          5 * LAKE_FIXTURE_SCALE + LAKE_FIXTURE_SCALE / 2;
        if (!(combined->ocean[left_index] || combined->lake[left_index]) ||
            !(combined->ocean[right_index] || combined->lake[right_index]))
            *shared_boundary_visible = 0;
    }
    return ocean_pixels > 0 && lake_pixels > 0;
}

static int adjacent_ocean_lake_fixture(FILE *summary) {
    unsigned char baseline_source[10 * 6] = {0};
    unsigned char right_lake_source[10 * 6] = {0};
    unsigned char left_lake_source[10 * 6] = {0};
    LakeRasterResult baseline;
    LakeRasterResult right_lake;
    LakeRasterResult left_lake;
    int right_ocean_unchanged, right_cross_clear, right_boundary;
    int left_ocean_unchanged, left_cross_clear, left_boundary;
    int right_contract;
    int left_contract;
    int x;
    int y;
    int ok;
    for (y = 1; y <= 4; y++) {
        for (x = 1; x <= 8; x++) {
            int index = y * 10 + x;
            baseline_source[index] = RENDER_WATER_CATEGORY_OCEAN;
            right_lake_source[index] = x <= 4 ?
                RENDER_WATER_CATEGORY_OCEAN : RENDER_WATER_CATEGORY_LAKE;
            left_lake_source[index] = x <= 4 ?
                RENDER_WATER_CATEGORY_LAKE : RENDER_WATER_CATEGORY_OCEAN;
        }
    }
    if (!rasterize_fixture(baseline_source, 10, 6, &baseline) ||
        !rasterize_fixture(right_lake_source, 10, 6, &right_lake) ||
        !rasterize_fixture(left_lake_source, 10, 6, &left_lake)) return 0;
    right_contract = adjacent_category_contract(
        right_lake_source, 10, &baseline, &right_lake,
        &right_ocean_unchanged, &right_cross_clear, &right_boundary);
    left_contract = adjacent_category_contract(
        left_lake_source, 10, &baseline, &left_lake,
        &left_ocean_unchanged, &left_cross_clear, &left_boundary);
    ok = right_contract && left_contract &&
         right_ocean_unchanged && left_ocean_unchanged &&
         right_cross_clear && left_cross_clear &&
         right_boundary && left_boundary;
    fprintf(summary,
            "case=lake_semantics_adjacent_ocean ok=%d "
            "ocean_unchanged=%d/%d cross_category_clear=%d/%d "
            "shared_boundary_visible=%d/%d overlap_removed=%llu/%llu "
            "hash=%llu/%llu\n",
            ok, right_ocean_unchanged, left_ocean_unchanged,
            right_cross_clear, left_cross_clear,
            right_boundary, left_boundary,
            (unsigned long long)right_lake.metrics.ocean_overlap_pixels_removed,
            (unsigned long long)left_lake.metrics.ocean_overlap_pixels_removed,
            (unsigned long long)raster_hash(&right_lake),
            (unsigned long long)raster_hash(&left_lake));
    return ok;
}

int game_presentation_lake_semantics_probe(FILE *summary) {
    int diagonal_ok;
    int holes_ok;
    int ocean_ok;
    int adjacent_ok;
    if (!summary) return 0;
    diagonal_ok = diagonal_fixture(summary);
    holes_ok = land_hole_fixture(summary);
    ocean_ok = separated_ocean_fixture(summary);
    adjacent_ok = adjacent_ocean_lake_fixture(summary);
    fprintf(summary,
            "case=lake_semantics_overall ok=%d diagonal=%d land_holes=%d "
            "separated_ocean=%d adjacent_ocean=%d scale=%d subsamples=%d\n",
            diagonal_ok && holes_ok && ocean_ok && adjacent_ok,
            diagonal_ok, holes_ok, ocean_ok, adjacent_ok,
            LAKE_FIXTURE_SCALE, LAKE_FIXTURE_SUBSAMPLES);
    return diagonal_ok && holes_ok && ocean_ok && adjacent_ok;
}
