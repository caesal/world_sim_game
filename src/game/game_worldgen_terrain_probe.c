#include "game_worldgen_terrain_probe.h"
#include "core/world_types.h"
#include "world/mountain_gen.h"
#include "world/world_gen_context.h"
#include "world/world_gen_elevation.h"
#include "world/world_gen_rng.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
enum {
    TERRAIN_PROBE_SMALL_W = 576, TERRAIN_PROBE_SMALL_H = 400,
    TERRAIN_PROBE_MEDIUM_W = 720, TERRAIN_PROBE_MEDIUM_H = 500,
    TERRAIN_PROBE_LARGE_W = 864, TERRAIN_PROBE_LARGE_H = 600,
    TERRAIN_PROBE_PROFILE_LIMIT = 512,
    TERRAIN_PROBE_PROFILE_RADIUS = 28
};
typedef struct {
    int width, height;
    int land, expected_land, elevation_errors, isolated_land, enclosed_ocean;
    int ocean_uplift_errors;
    int footprint, foothill, shoulder, core, isolated_core;
    int geography_high, geography_mid, geography_plain;
    int high_mid_edges, mid_plain_edges;
    int span_count, min_span, max_span, span_kinds;
    int peak_samples, graded_peaks, cardinal_samples, diagonal_samples;
    int chains;
    uint64_t uplift_sum, span_sum, cardinal_radius, diagonal_radius;
    uint64_t raster_hash, diagnostic_hash;
} TerrainProbeMetrics;
static uint64_t hash_u64(uint64_t hash, uint64_t value) {
    int byte_index;
    for (byte_index = 0; byte_index < 8; byte_index++) {
        hash ^= (uint8_t)(value >> (byte_index * 8));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}
static int expected_land_count(const WorldGenContext *context) {
    return world_gen_land_mask_target_tiles(
        context->config.ocean, context->tile_count);
}
static int neighbor_land_count(const WorldGenContext *context, int x, int y) {
    int count = 0;
    int dx, dy;
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int next;
            if ((dx == 0 && dy == 0) ||
                !world_gen_context_in_bounds(context, x + dx, y + dy)) continue;
            next = world_gen_context_index(context, x + dx, y + dy);
            if (context->land_mask[next]) count++;
        }
    }
    return count;
}
static int neighbor_uplift_count(const WorldGenContext *context, int x, int y) {
    int count = 0;
    int dx, dy;
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int next;
            if ((dx == 0 && dy == 0) ||
                !world_gen_context_in_bounds(context, x + dx, y + dy)) continue;
            next = world_gen_context_index(context, x + dx, y + dy);
            if (context->mountain_uplift[next] >= 7) count++;
        }
    }
    return count;
}
static int geography_band(uint8_t geography) {
    if (geography == GEO_MOUNTAIN || geography == GEO_VOLCANO) return 2;
    if (geography == GEO_HILL || geography == GEO_PLATEAU) return 1;
    if (geography == GEO_PLAIN || geography == GEO_BASIN) return 0;
    return -1;
}
static int popcount32(uint32_t value) {
    int count = 0;
    while (value) {
        count += (int)(value & 1u);
        value >>= 1;
    }
    return count;
}
static void collect_spans(const WorldGenContext *context, TerrainProbeMetrics *metrics) {
    uint32_t span_mask = 0;
    int y;
    metrics->min_span = INT_MAX;
    for (y = 0; y < context->height; y++) {
        int x = 0;
        while (x < context->width) {
            int start;
            int length;
            while (x < context->width &&
                   context->mountain_uplift[world_gen_context_index(context, x, y)] <= 0) x++;
            start = x;
            while (x < context->width &&
                   context->mountain_uplift[world_gen_context_index(context, x, y)] > 0) x++;
            length = x - start;
            if (length <= 0) continue;
            metrics->span_count++;
            metrics->span_sum += (uint64_t)length;
            if (length < metrics->min_span) metrics->min_span = length;
            if (length > metrics->max_span) metrics->max_span = length;
            span_mask |= UINT32_C(1) << (length < 31 ? length : 31);
        }
    }
    if (metrics->span_count == 0) metrics->min_span = 0;
    metrics->span_kinds = popcount32(span_mask);
}
static int is_local_peak(const WorldGenContext *context, int index) {
    int x = index % context->width;
    int y = index / context->width;
    int value = context->mountain_uplift[index];
    int lower = 0;
    int dx, dy;
    if (value < 18) return 0;
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int next;
            if ((dx == 0 && dy == 0) ||
                !world_gen_context_in_bounds(context, x + dx, y + dy)) continue;
            next = world_gen_context_index(context, x + dx, y + dy);
            if (context->mountain_uplift[next] > value) return 0;
            if (context->mountain_uplift[next] < value) lower = 1;
        }
    }
    return lower;
}
static void collect_peak_profiles(const WorldGenContext *context,
                                  TerrainProbeMetrics *metrics) {
    static const int dx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    static const int dy[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    int index;
    for (index = 0; index < context->tile_count; index++) {
        int x, y, direction;
        int graded_rays = 0;
        if (!is_local_peak(context, index)) continue;
        if (metrics->peak_samples >= TERRAIN_PROBE_PROFILE_LIMIT) continue;
        metrics->peak_samples++;
        x = index % context->width;
        y = index / context->width;
        for (direction = 0; direction < 8; direction++) {
            int saw_shoulder = 0;
            int saw_foothill = 0;
            int last_positive = 0;
            int radius;
            for (radius = 1; radius <= TERRAIN_PROBE_PROFILE_RADIUS; radius++) {
                int nx = x + dx[direction] * radius;
                int ny = y + dy[direction] * radius;
                int uplift;
                if (!world_gen_context_in_bounds(context, nx, ny)) break;
                uplift = context->mountain_uplift[world_gen_context_index(context, nx, ny)];
                if (uplift <= 0) break;
                last_positive = radius;
                if (uplift >= 7 && uplift < 18) saw_shoulder = 1;
                if (uplift > 0 && uplift < 7) saw_foothill = 1;
            }
            if (saw_shoulder && saw_foothill) graded_rays++;
            if (last_positive <= 0) continue;
            if ((direction & 1) == 0) {
                metrics->cardinal_radius += (uint64_t)last_positive * 100u;
                metrics->cardinal_samples++;
            } else {
                metrics->diagonal_radius += (uint64_t)last_positive * 141u;
                metrics->diagonal_samples++;
            }
        }
        if (graded_rays >= 3) metrics->graded_peaks++;
    }
}
static void collect_metrics(const WorldGenContext *context, TerrainProbeMetrics *metrics) {
    int y, x;
    memset(metrics, 0, sizeof(*metrics));
    metrics->width = context->width;
    metrics->height = context->height;
    metrics->expected_land = expected_land_count(context);
    metrics->chains = world_mountain_chain_count();
    metrics->diagnostic_hash = world_gen_last_diagnostics()->physical_hash;
    metrics->raster_hash = UINT64_C(1469598103934665603);
    for (y = 0; y < context->height; y++) {
        for (x = 0; x < context->width; x++) {
            int index = world_gen_context_index(context, x, y);
            int land = context->land_mask[index] != 0;
            int uplift = context->mountain_uplift[index];
            int band = geography_band(context->geography[index]);
            int neighbors = neighbor_land_count(context, x, y);
            uint64_t physical = (uint16_t)context->elevation[index] |
                                ((uint64_t)(uint16_t)uplift << 16) |
                                ((uint64_t)context->land_mask[index] << 32) |
                                ((uint64_t)context->geography[index] << 40);
            metrics->raster_hash = hash_u64(metrics->raster_hash, physical);
            metrics->land += land;
            if ((land && context->elevation[index] <= context->sea_level) ||
                (!land && context->elevation[index] >= context->sea_level)) {
                metrics->elevation_errors++;
            }
            /* Only threshold-selected coastal lowlands are topology repair
               products. Preserve genuine above-cut small islands/peninsulas. */
            if (land && context->coastal_lowland_hint[index] && neighbors <= 1)
                metrics->isolated_land++;
            if (!land && x > 0 && y > 0 && x + 1 < context->width &&
                y + 1 < context->height && neighbors == 8) metrics->enclosed_ocean++;
            if (!land && (uplift != 0 || context->elevation[index] != context->base_elevation[index])) {
                metrics->ocean_uplift_errors++;
            }
            if (uplift > 0) {
                metrics->footprint++;
                metrics->uplift_sum += (uint64_t)uplift;
                if (uplift < 7) metrics->foothill++;
                else if (uplift < 18) metrics->shoulder++;
                else {
                    metrics->core++;
                    if (neighbor_uplift_count(context, x, y) < 3) metrics->isolated_core++;
                }
            }
            if (band == 2) metrics->geography_high++;
            else if (band == 1) metrics->geography_mid++;
            else if (band == 0) metrics->geography_plain++;
            if (x + 1 < context->width) {
                int next_band = geography_band(context->geography[index + 1]);
                if ((band == 2 && next_band == 1) || (band == 1 && next_band == 2)) {
                    metrics->high_mid_edges++;
                }
                if ((band == 1 && next_band == 0) || (band == 0 && next_band == 1)) {
                    metrics->mid_plain_edges++;
                }
            }
            if (y + 1 < context->height) {
                int next_band = geography_band(context->geography[index + context->width]);
                if ((band == 2 && next_band == 1) || (band == 1 && next_band == 2)) {
                    metrics->high_mid_edges++;
                }
                if ((band == 1 && next_band == 0) || (band == 0 && next_band == 1)) {
                    metrics->mid_plain_edges++;
                }
            }
        }
    }
    collect_spans(context, metrics);
    collect_peak_profiles(context, metrics);
}
static WorldGenContext *prepare_context(FILE *file, const char *label,
                                        const WorldGenConfig *config, int width, int height) {
    WorldGenConfig active = *config;
    WorldGenContext *context = (WorldGenContext *)calloc(1, sizeof(*context));
    active.random_seed = 0;
    if (!context || !world_gen_context_create(context, &active, width, height, active.seed) ||
        !world_gen_run_prepared(context)) {
        const WorldGenDiagnostics *diagnostics = world_gen_last_diagnostics();
        fprintf(file, "case=terrain_prepare label=%s map=%dx%d required_paths=%d ok=0\n",
                label, width, height,
                diagnostics ? diagnostics->river_segments_required : -1);
        world_gen_release_prepared(context);
        return NULL;
    }
    return context;
}
static int topology_ok(const TerrainProbeMetrics *metrics) {
    return metrics->land == metrics->expected_land && metrics->elevation_errors == 0 &&
           metrics->isolated_land == 0 && metrics->enclosed_ocean == 0 &&
           metrics->ocean_uplift_errors == 0;
}
static int shape_ok(const TerrainProbeMetrics *metrics) {
    uint64_t cardinal_mean = metrics->cardinal_samples > 0
        ? metrics->cardinal_radius / (uint64_t)metrics->cardinal_samples : 0;
    uint64_t diagonal_mean = metrics->diagonal_samples > 0
        ? metrics->diagonal_radius / (uint64_t)metrics->diagonal_samples : 0;
    int radial_ok = cardinal_mean > 0 && diagonal_mean > 0 &&
                    cardinal_mean * 5u >= diagonal_mean * 2u &&
                    diagonal_mean * 5u >= cardinal_mean * 2u;
    return metrics->core > 0 && metrics->shoulder > 0 && metrics->foothill > 0 &&
           metrics->isolated_core * 10 <= metrics->core + 9 &&
           metrics->geography_high > 0 && metrics->geography_mid > 0 &&
           metrics->geography_plain > 0 && metrics->high_mid_edges > 0 &&
           metrics->mid_plain_edges > 0 && metrics->max_span >= 6 &&
           metrics->span_kinds >= 3 && metrics->peak_samples > 0 &&
           metrics->graded_peaks > 0 && radial_ok;
}
static int run_case(FILE *file, const char *label, const WorldGenConfig *config,
                    int width, int height, int require_shape,
                    TerrainProbeMetrics *out_metrics) {
    WorldGenContext *context = prepare_context(file, label, config, width, height);
    TerrainProbeMetrics metrics;
    int ok;
    if (!context) return 0;
    collect_metrics(context, &metrics);
    ok = topology_ok(&metrics) && (!require_shape || shape_ok(&metrics));
    fprintf(file, "case=terrain_world label=%s map=%dx%d land=%d/%d elevation_errors=%d isolated=%d holes=%d "
                  "ocean_uplift=%d footprint=%d core=%d shoulder=%d foothill=%d chains=%d "
                  "span=%d..%d kinds=%d peaks=%d/%d radial=%llu/%d,%llu/%d geo_edges=%d/%d "
                  "hash=%016llx raster=%016llx ok=%d\n",
            label, width, height, metrics.land, metrics.expected_land,
            metrics.elevation_errors, metrics.isolated_land, metrics.enclosed_ocean, metrics.ocean_uplift_errors,
            metrics.footprint, metrics.core, metrics.shoulder, metrics.foothill,
            metrics.chains, metrics.min_span, metrics.max_span, metrics.span_kinds,
            metrics.graded_peaks, metrics.peak_samples,
            (unsigned long long)metrics.cardinal_radius, metrics.cardinal_samples,
            (unsigned long long)metrics.diagonal_radius, metrics.diagonal_samples, metrics.high_mid_edges,
            metrics.mid_plain_edges, (unsigned long long)metrics.diagnostic_hash,
            (unsigned long long)metrics.raster_hash, ok);
    if (out_metrics) *out_metrics = metrics;
    world_gen_release_prepared(context);
    return ok;
}
static uint64_t mask_hash(const WorldGenContext *context) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int i;
    for (i = 0; i < context->tile_count; i++) hash = hash_u64(hash, context->land_mask[i]);
    return hash;
}
static uint64_t ocean_elevation_hash(const WorldGenContext *context) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int i;
    for (i = 0; i < context->tile_count; i++) {
        if (!context->land_mask[i]) hash = hash_u64(hash, (uint16_t)context->elevation[i]);
    }
    return hash;
}
static int check_coastline_stage(FILE *file, const WorldGenConfig *config) {
    WorldGenContext *context = (WorldGenContext *)calloc(1, sizeof(*context));
    WorldGenConfig active = *config;
    uint64_t mask_before = 0, ocean_before = 0;
    uint64_t mask_after = 0, ocean_after = 0;
    int ocean_errors = 0;
    int ok = 0;
    int i;
    active.random_seed = 0;
    if (context && world_gen_context_create(context, &active, TERRAIN_PROBE_SMALL_W,
                                             TERRAIN_PROBE_SMALL_H, active.seed) &&
        world_gen_build_elevation_and_mask(context)) {
        mask_before = mask_hash(context);
        ocean_before = ocean_elevation_hash(context);
        if (world_gen_apply_mountains(context)) {
            world_gen_finalize_elevation(context);
            mask_after = mask_hash(context);
            ocean_after = ocean_elevation_hash(context);
            for (i = 0; i < context->tile_count; i++) {
                if (!context->land_mask[i] &&
                    (context->mountain_uplift[i] != 0 ||
                     context->elevation[i] != context->base_elevation[i])) ocean_errors++;
            }
            ok = mask_before == mask_after && ocean_before == ocean_after && ocean_errors == 0;
        }
    }
    fprintf(file, "case=terrain_coastline_stage mask_same=%d ocean_same=%d "
                  "ocean_uplift_errors=%d before=%016llx after=%016llx ok=%d\n",
            mask_before != 0 && mask_before == mask_after,
            ocean_before != 0 && ocean_before == ocean_after, ocean_errors,
            (unsigned long long)mask_before, (unsigned long long)mask_after, ok);
    if (context) {
        world_gen_context_destroy(context);
        free(context);
    }
    return ok;
}
static uint64_t stream_signature(uint32_t seed) {
    WorldGenRng rng;
    uint64_t hash = UINT64_C(1469598103934665603);
    int i;
    world_gen_rng_init(&rng, seed);
    for (i = 0; i < 32; i++) hash = hash_u64(hash, world_gen_rng_next(&rng));
    return hash;
}
static int check_stage_streams(FILE *file, uint32_t master_seed) {
    uint32_t seeds[WORLD_GEN_PHASE_COUNT];
    uint64_t signatures[WORLD_GEN_PHASE_COUNT];
    WorldGenRng mountain_consumption;
    uint64_t climate_before, climate_after, downstream_repeat;
    int distinct = 1;
    int phase, other;
    for (phase = 0; phase < WORLD_GEN_PHASE_COUNT; phase++) {
        seeds[phase] = world_gen_derive_seed(master_seed, (uint32_t)phase + 1u);
        signatures[phase] = stream_signature(seeds[phase]);
        for (other = 0; other < phase; other++) {
            if (seeds[phase] == seeds[other] || signatures[phase] == signatures[other]) distinct = 0;
        }
    }
    climate_before = signatures[WORLD_GEN_PHASE_CLIMATE];
    world_gen_rng_init(&mountain_consumption, seeds[WORLD_GEN_PHASE_MOUNTAIN]);
    for (phase = 0; phase < 4096; phase++) (void)world_gen_rng_next(&mountain_consumption);
    climate_after = stream_signature(seeds[WORLD_GEN_PHASE_CLIMATE]);
    downstream_repeat = stream_signature(world_gen_derive_seed(master_seed,
                                           WORLD_GEN_PHASE_DOWNSTREAM + 1u));
    {
        int ok = distinct && climate_before == climate_after &&
                 signatures[WORLD_GEN_PHASE_DOWNSTREAM] == downstream_repeat;
        fprintf(file, "case=terrain_stage_rng distinct=%d mountain_draws=4096 "
                      "climate_independent=%d downstream_repeat=%d signature=%016llx ok=%d\n",
                distinct, climate_before == climate_after,
                signatures[WORLD_GEN_PHASE_DOWNSTREAM] == downstream_repeat,
                (unsigned long long)downstream_repeat, ok);
        return ok;
    }
}
int game_worldgen_terrain_probe_run(FILE *file, const WorldGenConfig *base_config) {
    TerrainProbeMetrics a1 = {0}, a2 = {0}, b = {0}, a3 = {0};
    TerrainProbeMetrics relief_low = {0}, relief_high = {0}, bias_low = {0}, bias_high = {0};
    TerrainProbeMetrics medium = {0}, large = {0};
    WorldGenConfig config;
    int ok = 1;
    int deterministic, relief_response, bias_response, size_variation;
    if (!file || !base_config) return 0;
    config = *base_config;
    config.random_seed = 0;
    ok &= check_coastline_stage(file, &config);
    ok &= check_stage_streams(file, config.seed);
    ok &= run_case(file, "terrain_a1", &config, TERRAIN_PROBE_SMALL_W,
                   TERRAIN_PROBE_SMALL_H, 1, &a1);
    ok &= run_case(file, "terrain_a2", &config, TERRAIN_PROBE_SMALL_W,
                   TERRAIN_PROBE_SMALL_H, 0, &a2);
    config.seed = base_config->seed + 1u;
    ok &= run_case(file, "terrain_b", &config, TERRAIN_PROBE_SMALL_W,
                   TERRAIN_PROBE_SMALL_H, 0, &b);
    config = *base_config;
    config.random_seed = 0;
    ok &= run_case(file, "terrain_a3", &config, TERRAIN_PROBE_SMALL_W,
                   TERRAIN_PROBE_SMALL_H, 0, &a3);
    deterministic = a1.diagnostic_hash != 0 && a1.diagnostic_hash == a2.diagnostic_hash &&
                    a1.diagnostic_hash == a3.diagnostic_hash &&
                    a1.raster_hash == a2.raster_hash && a1.raster_hash == a3.raster_hash &&
                    a1.diagnostic_hash != b.diagnostic_hash && a1.raster_hash != b.raster_hash;
    fprintf(file, "case=terrain_determinism fresh=%d aba=%d distinct_b=%d "
                  "raster_fresh=%d raster_aba=%d ok=%d\n",
            a1.diagnostic_hash == a2.diagnostic_hash, a1.diagnostic_hash == a3.diagnostic_hash,
            a1.diagnostic_hash != b.diagnostic_hash, a1.raster_hash == a2.raster_hash,
            a1.raster_hash == a3.raster_hash, deterministic);
    ok &= deterministic;

    config = *base_config;
    config.random_seed = 0;
    config.relief = 20;
    config.bias_mountain = 50;
    ok &= run_case(file, "relief_low", &config, TERRAIN_PROBE_SMALL_W,
                   TERRAIN_PROBE_SMALL_H, 0, &relief_low);
    config.relief = 80;
    ok &= run_case(file, "relief_high", &config, TERRAIN_PROBE_SMALL_W,
                   TERRAIN_PROBE_SMALL_H, 1, &relief_high);
    relief_response = relief_high.uplift_sum > relief_low.uplift_sum &&
                      relief_high.core >= relief_low.core &&
                      relief_high.chains == relief_low.chains &&
                      relief_high.diagnostic_hash != relief_low.diagnostic_hash;
    fprintf(file, "case=terrain_relief_response fixed_bias=50 low_sum=%llu high_sum=%llu "
                  "low_core=%d high_core=%d low_chains=%d high_chains=%d same_seed_hash_changed=%d ok=%d\n",
            (unsigned long long)relief_low.uplift_sum,
            (unsigned long long)relief_high.uplift_sum, relief_low.core, relief_high.core,
            relief_low.chains, relief_high.chains,
            relief_low.diagnostic_hash != relief_high.diagnostic_hash, relief_response);
    ok &= relief_response;

    config = *base_config;
    config.random_seed = 0;
    config.relief = 50;
    config.bias_mountain = 20;
    ok &= run_case(file, "bias_low", &config, TERRAIN_PROBE_SMALL_W,
                   TERRAIN_PROBE_SMALL_H, 0, &bias_low);
    config.bias_mountain = 80;
    ok &= run_case(file, "bias_high", &config, TERRAIN_PROBE_SMALL_W,
                   TERRAIN_PROBE_SMALL_H, 1, &bias_high);
    bias_response = bias_high.footprint > bias_low.footprint &&
                    bias_high.uplift_sum > bias_low.uplift_sum &&
                    bias_high.chains > bias_low.chains &&
                    bias_high.span_sum * (uint64_t)bias_low.span_count >
                        bias_low.span_sum * (uint64_t)bias_high.span_count &&
                    bias_high.diagnostic_hash != bias_low.diagnostic_hash;
    fprintf(file, "case=terrain_bias_response fixed_relief=50 low_coverage=%d high_coverage=%d "
                  "low_chains=%d high_chains=%d low_span=%llu/%d high_span=%llu/%d "
                  "same_seed_hash_changed=%d ok=%d\n",
            bias_low.footprint, bias_high.footprint, bias_low.chains, bias_high.chains,
            (unsigned long long)bias_low.span_sum, bias_low.span_count,
            (unsigned long long)bias_high.span_sum, bias_high.span_count,
            bias_low.diagnostic_hash != bias_high.diagnostic_hash, bias_response);
    ok &= bias_response;

    config = *base_config;
    config.random_seed = 0;
    ok &= run_case(file, "terrain_medium", &config, TERRAIN_PROBE_MEDIUM_W,
                   TERRAIN_PROBE_MEDIUM_H, 1, &medium);
    ok &= run_case(file, "terrain_large", &config, TERRAIN_PROBE_LARGE_W,
                   TERRAIN_PROBE_LARGE_H, 1, &large);
    size_variation = medium.diagnostic_hash != 0 && large.diagnostic_hash != 0 &&
                     medium.diagnostic_hash != large.diagnostic_hash &&
                     medium.raster_hash != large.raster_hash &&
                     large.land > medium.land && large.footprint > medium.footprint;
    fprintf(file, "case=terrain_size_variation medium=%dx%d large=%dx%d "
                  "land=%d/%d footprint=%d/%d hash_distinct=%d ok=%d\n",
            medium.width, medium.height, large.width, large.height,
            medium.land, large.land, medium.footprint, large.footprint,
            medium.diagnostic_hash != large.diagnostic_hash, size_variation);
    ok &= size_variation;
    fprintf(file, "case=terrain_probe proxies=radial_uplift,row_span,phase_streams ok=%d\n", ok);
    return ok;
}
