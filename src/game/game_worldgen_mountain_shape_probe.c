#include "game_worldgen_mountain_shape_probe.h"

#include "world/mountain_gen.h"
#include "world/world_gen_context.h"
#include "world/world_gen_elevation.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    MOUNTAIN_SHAPE_SMALL_W = 576,
    MOUNTAIN_SHAPE_SMALL_H = 400,
    MOUNTAIN_SHAPE_MEDIUM_W = 720,
    MOUNTAIN_SHAPE_MEDIUM_H = 500,
    MOUNTAIN_SHAPE_RANGE_SPAN = 8,
    MOUNTAIN_SHAPE_MAX_THIN_RADIUS = 8
};

typedef struct {
    uint8_t *mask;
    uint8_t *thick;
    uint8_t *seen;
    int32_t *distance;
    int32_t *queue;
} MountainShapeBuffers;

typedef struct {
    int tiles;
    int components;
    int range_components;
    int short_fragments;
    int singleton_components;
    int thick_tiles;
    int line_only_ranges;
    int overlong_thin_ranges;
    int max_component_span;
    int max_thin_radius;
} MountainShapeMetrics;

static int allocate_buffers(MountainShapeBuffers *buffers, int tile_count) {
    if (!buffers || tile_count <= 0) return 0;
    memset(buffers, 0, sizeof(*buffers));
    buffers->mask = (uint8_t *)calloc((size_t)tile_count, sizeof(*buffers->mask));
    buffers->thick = (uint8_t *)calloc((size_t)tile_count, sizeof(*buffers->thick));
    buffers->seen = (uint8_t *)calloc((size_t)tile_count, sizeof(*buffers->seen));
    buffers->distance = (int32_t *)malloc((size_t)tile_count * sizeof(*buffers->distance));
    buffers->queue = (int32_t *)malloc((size_t)tile_count * sizeof(*buffers->queue));
    return buffers->mask && buffers->thick && buffers->seen &&
           buffers->distance && buffers->queue;
}

static void free_buffers(MountainShapeBuffers *buffers) {
    if (!buffers) return;
    free(buffers->mask);
    free(buffers->thick);
    free(buffers->seen);
    free(buffers->distance);
    free(buffers->queue);
    memset(buffers, 0, sizeof(*buffers));
}

static void build_layer_mask(const WorldGenContext *context, int minimum_uplift,
                             MountainShapeBuffers *buffers) {
    int index;
    memset(buffers->mask, 0, (size_t)context->tile_count);
    memset(buffers->thick, 0, (size_t)context->tile_count);
    memset(buffers->seen, 0, (size_t)context->tile_count);
    for (index = 0; index < context->tile_count; index++) {
        buffers->distance[index] = -1;
        if (context->land_mask[index] &&
            context->mountain_uplift[index] >= minimum_uplift) {
            buffers->mask[index] = 1;
        }
    }
}

static void mark_two_by_two_thickness(const WorldGenContext *context,
                                      MountainShapeBuffers *buffers) {
    int x;
    int y;
    for (y = 0; y + 1 < context->height; y++) {
        for (x = 0; x + 1 < context->width; x++) {
            int a = world_gen_context_index(context, x, y);
            int b = a + 1;
            int c = a + context->width;
            int d = c + 1;
            if (!buffers->mask[a] || !buffers->mask[b] ||
                !buffers->mask[c] || !buffers->mask[d]) continue;
            buffers->thick[a] = 1;
            buffers->thick[b] = 1;
            buffers->thick[c] = 1;
            buffers->thick[d] = 1;
        }
    }
}

static void build_thin_distances(const WorldGenContext *context,
                                 MountainShapeBuffers *buffers) {
    static const int dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const int dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    int head = 0;
    int tail = 0;
    int index;
    for (index = 0; index < context->tile_count; index++) {
        if (!buffers->thick[index]) continue;
        buffers->distance[index] = 0;
        buffers->queue[tail++] = index;
    }
    while (head < tail) {
        int current = buffers->queue[head++];
        int x = current % context->width;
        int y = current / context->width;
        int direction;
        for (direction = 0; direction < 8; direction++) {
            int nx = x + dx[direction];
            int ny = y + dy[direction];
            int next;
            if (!world_gen_context_in_bounds(context, nx, ny)) continue;
            next = world_gen_context_index(context, nx, ny);
            if (!buffers->mask[next] || buffers->distance[next] >= 0) continue;
            buffers->distance[next] = buffers->distance[current] + 1;
            buffers->queue[tail++] = next;
        }
    }
}

static void collect_component(const WorldGenContext *context, int start,
                              MountainShapeBuffers *buffers,
                              MountainShapeMetrics *metrics) {
    static const int dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const int dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    int min_x = start % context->width;
    int max_x = min_x;
    int min_y = start / context->width;
    int max_y = min_y;
    int component_tiles = 0;
    int component_thick = 0;
    int component_thin_run = 0;
    int head = 0;
    int tail = 0;
    buffers->seen[start] = 1;
    buffers->queue[tail++] = start;
    while (head < tail) {
        int current = buffers->queue[head++];
        int x = current % context->width;
        int y = current / context->width;
        int direction;
        component_tiles++;
        component_thick += buffers->thick[current] != 0;
        if (buffers->distance[current] > component_thin_run) {
            component_thin_run = buffers->distance[current];
        }
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
        for (direction = 0; direction < 8; direction++) {
            int nx = x + dx[direction];
            int ny = y + dy[direction];
            int next;
            if (!world_gen_context_in_bounds(context, nx, ny)) continue;
            next = world_gen_context_index(context, nx, ny);
            if (!buffers->mask[next] || buffers->seen[next]) continue;
            buffers->seen[next] = 1;
            buffers->queue[tail++] = next;
        }
    }
    {
        int width = max_x - min_x + 1;
        int height = max_y - min_y + 1;
        int span = width > height ? width : height;
        metrics->components++;
        if (component_tiles == 1) metrics->singleton_components++;
        if (span > metrics->max_component_span) metrics->max_component_span = span;
        if (span < MOUNTAIN_SHAPE_RANGE_SPAN) {
            metrics->short_fragments++;
            return;
        }
        metrics->range_components++;
        if (component_thick == 0) metrics->line_only_ranges++;
        if (component_thin_run > MOUNTAIN_SHAPE_MAX_THIN_RADIUS) {
            metrics->overlong_thin_ranges++;
        }
        if (component_thin_run > metrics->max_thin_radius) {
            metrics->max_thin_radius = component_thin_run;
        }
    }
}

static int analyze_layer(const WorldGenContext *context, int minimum_uplift,
                         MountainShapeBuffers *buffers,
                         MountainShapeMetrics *metrics) {
    int index;
    memset(metrics, 0, sizeof(*metrics));
    build_layer_mask(context, minimum_uplift, buffers);
    mark_two_by_two_thickness(context, buffers);
    build_thin_distances(context, buffers);
    for (index = 0; index < context->tile_count; index++) {
        metrics->tiles += buffers->mask[index] != 0;
        metrics->thick_tiles += buffers->thick[index] != 0;
        if (buffers->mask[index] && !buffers->seen[index]) {
            collect_component(context, index, buffers, metrics);
        }
    }
    return metrics->tiles > 0 && metrics->range_components > 0 &&
           metrics->line_only_ranges == 0 && metrics->overlong_thin_ranges == 0;
}

static WorldGenContext *prepare_mountain_context(const WorldGenConfig *config,
                                                 int width, int height) {
    WorldGenContext *context = (WorldGenContext *)calloc(1, sizeof(*context));
    if (!context ||
        !world_gen_context_create(context, config, width, height, config->seed) ||
        !world_gen_build_elevation_and_mask(context) ||
        !world_gen_apply_mountains(context)) {
        if (context) {
            world_gen_context_destroy(context);
            free(context);
        }
        return NULL;
    }
    world_gen_finalize_elevation(context);
    return context;
}

static int run_case(FILE *file, const char *label, const WorldGenConfig *config,
                    int width, int height) {
    static const int thresholds[3] = {1, 7, 18};
    static const char *names[3] = {"footprint", "shoulder", "core"};
    WorldGenContext *context = prepare_mountain_context(config, width, height);
    MountainShapeBuffers buffers = {0};
    int layer;
    int ok = context != NULL;
    if (!context) {
        fprintf(file, "case=mountain_shape_prepare label=%s map=%dx%d seed=%u ok=0\n",
                label, width, height, config->seed);
        return 0;
    }
    if (!allocate_buffers(&buffers, context->tile_count)) {
        fprintf(file, "case=mountain_shape_alloc label=%s map=%dx%d ok=0\n",
                label, width, height);
        world_gen_context_destroy(context);
        free(context);
        free_buffers(&buffers);
        return 0;
    }
    for (layer = 0; layer < 3; layer++) {
        MountainShapeMetrics metrics;
        int strict_shape_ok = analyze_layer(context, thresholds[layer], &buffers, &metrics);
        int layer_ok = layer < 2 ? strict_shape_ok :
            metrics.tiles > 0 && metrics.range_components > 0;
        fprintf(file, "case=mountain_shape label=%s layer=%s threshold=%d map=%dx%d "
                      "seed=%u tiles=%d thick=%d components=%d ranges=%d fragments=%d "
                      "singletons=%d max_span=%d max_thin_radius=%d line_ranges=%d "
                      "long_thin=%d strict_envelope=%d ok=%d\n",
                label, names[layer], thresholds[layer], width, height, config->seed,
                metrics.tiles, metrics.thick_tiles, metrics.components,
                metrics.range_components, metrics.short_fragments,
                metrics.singleton_components, metrics.max_component_span,
                metrics.max_thin_radius, metrics.line_only_ranges,
                metrics.overlong_thin_ranges, layer < 2, layer_ok);
        ok &= layer_ok;
    }
    free_buffers(&buffers);
    world_gen_context_destroy(context);
    free(context);
    return ok;
}

int game_worldgen_mountain_shape_probe_run(FILE *file,
                                           const WorldGenConfig *base_config) {
    WorldGenConfig config;
    int ok = 1;
    if (!file || !base_config) return 0;
    config = *base_config;
    config.random_seed = 0;
    ok &= run_case(file, "default", &config,
                   MOUNTAIN_SHAPE_SMALL_W, MOUNTAIN_SHAPE_SMALL_H);
    config = *base_config;
    config.random_seed = 0;
    config.seed += 101u;
    config.relief = 20;
    config.bias_mountain = 80;
    ok &= run_case(file, "low_relief_high_bias", &config,
                   MOUNTAIN_SHAPE_SMALL_W, MOUNTAIN_SHAPE_SMALL_H);
    config = *base_config;
    config.random_seed = 0;
    config.seed += 202u;
    config.relief = 80;
    config.bias_mountain = 20;
    ok &= run_case(file, "high_relief_low_bias", &config,
                   MOUNTAIN_SHAPE_MEDIUM_W, MOUNTAIN_SHAPE_MEDIUM_H);
    fprintf(file, "case=mountain_shape_probe method=component_2x2_thickness "
                  "range_span=%d max_thin_radius=%d ok=%d\n",
            MOUNTAIN_SHAPE_RANGE_SPAN, MOUNTAIN_SHAPE_MAX_THIN_RADIUS, ok);
    return ok;
}
