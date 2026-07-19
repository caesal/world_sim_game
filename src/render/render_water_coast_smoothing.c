#include "render/render_water_coast_smoothing.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int area;
    int min_x, min_y, max_x, max_y;
    int attachment_mask;
    int direction;
    long long sum_x, sum_y;
    long long contact_x, contact_y;
    int contact_count;
    int cardinal_edges;
    int contact_groups;
    int anchor_x, anchor_y;
    int passage;
    int preserve;
    int sparse_mesh;
    int clustered;
} ThinComponent;

static int mask_at(const unsigned char *mask, int width, int height,
                   int x, int y) {
    return x >= 0 && y >= 0 && x < width && y < height &&
           mask[y * width + x] != 0;
}

static int ocean_or_edge(const unsigned char *mask, int width, int height,
                         int x, int y) {
    if (x < 0 || y < 0 || x >= width || y >= height) return 1;
    return mask[y * width + x] != 0;
}

static void build_opened_mask(const unsigned char *ocean, int width,
                              int height, int radius,
                              unsigned char *core,
                              unsigned char *opened) {
    int x, y, offset;
    /* A square opening is separable.  Four short one-dimensional passes keep
       the multi-scale fringe scan bounded while preserving the exact square
       morphology used by the original radius-one implementation. */
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            int solid = 1;
            for (offset = -radius; solid && offset <= radius; offset++)
                solid = ocean_or_edge(ocean, width, height,
                                      x + offset, y);
            core[y * width + x] = (unsigned char)solid;
        }
    }
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            int solid = 1;
            for (offset = -radius; solid && offset <= radius; offset++)
                solid = ocean_or_edge(core, width, height,
                                      x, y + offset);
            opened[y * width + x] = (unsigned char)solid;
        }
    }
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            int restored = 0;
            for (offset = -radius; !restored && offset <= radius; offset++)
                restored = mask_at(opened, width, height,
                                   x + offset, y);
            core[y * width + x] = (unsigned char)restored;
        }
    }
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            int restored = 0;
            if (mask_at(ocean, width, height, x, y)) {
                for (offset = -radius; !restored && offset <= radius;
                     offset++)
                    restored = mask_at(core, width, height,
                                       x, y + offset);
            }
            opened[y * width + x] = (unsigned char)restored;
        }
    }
}

static int append_component(ThinComponent **components, int *count,
                            int *capacity, ThinComponent component) {
    ThinComponent *replacement;
    int next;
    if (*count < *capacity) {
        (*components)[(*count)++] = component;
        return 1;
    }
    next = *capacity ? *capacity * 2 : 256;
    replacement = (ThinComponent *)realloc(
        *components, (size_t)next * sizeof(**components));
    if (!replacement) return 0;
    *components = replacement;
    *capacity = next;
    (*components)[(*count)++] = component;
    return 1;
}

static int vector_sector(long long dx, long long dy) {
    long long ax = llabs(dx);
    long long ay = llabs(dy);
    if (ax * 2 < ay) return dy < 0 ? 0 : 4;
    if (ay * 2 < ax) return dx > 0 ? 2 : 6;
    if (dx >= 0) return dy < 0 ? 1 : 3;
    return dy >= 0 ? 5 : 7;
}

static int sector_parallel(int left, int right) {
    int delta = abs(left - right);
    if (delta > 4) delta = 8 - delta;
    return delta <= 1 || delta >= 3;
}

static int major_span(const ThinComponent *component) {
    int width = component->max_x - component->min_x + 1;
    int height = component->max_y - component->min_y + 1;
    return width > height ? width : height;
}

static int compatible_size(const ThinComponent *left,
                           const ThinComponent *right) {
    int a = major_span(left);
    int b = major_span(right);
    return a <= b * 4 && b <= a * 4;
}

static int opposite_attachments(int mask) {
    return ((mask & (1 << 0)) && (mask & (1 << 4))) ||
           ((mask & (1 << 2)) && (mask & (1 << 6)));
}

static void classify_component(ThinComponent *component) {
    long long center_x;
    long long center_y;
    long long attach_x;
    long long attach_y;
    /* Preserve the existing north/south and east/west two-core passages. */
    component->passage = opposite_attachments(component->attachment_mask);
    component->preserve = component->passage;
    if (component->contact_count == 0) {
        int span_x = component->max_x - component->min_x + 1;
        int span_y = component->max_y - component->min_y + 1;
        int box_area = span_x * span_y;
        int cardinal_connected = component->area <= 1 ||
                                 component->cardinal_edges >=
                                     component->area - 1;
        int compact = span_x <= span_y * 2 && span_y <= span_x * 2 &&
                      component->area * 2 >= box_area &&
                      cardinal_connected;
        component->anchor_x = (component->min_x + component->max_x) / 2;
        component->anchor_y = (component->min_y + component->max_y) / 2;
        /* Area alone does not make detached fringe coherent water. Large,
           slender parallel pieces must still participate in comb detection;
           compact bodies and isolated non-clusters remain untouched. */
        if (compact) component->preserve = 1;
        component->sparse_mesh = component->area >= 6 &&
                                 !cardinal_connected;
        component->direction = span_x > span_y * 2 ? 2 :
                               (span_y > span_x * 2 ? 4 : 3);
        return;
    }
    center_x = component->sum_x / component->area;
    center_y = component->sum_y / component->area;
    attach_x = component->contact_x / component->contact_count;
    attach_y = component->contact_y / component->contact_count;
    component->anchor_x = (int)attach_x;
    component->anchor_y = (int)attach_y;
    component->direction = vector_sector(center_x - attach_x,
                                         center_y - attach_y);
    {
        int span_x = component->max_x - component->min_x + 1;
        int span_y = component->max_y - component->min_y + 1;
        int box_area = span_x * span_y;
        component->sparse_mesh =
            component->area >= WATER_COAST_SMOOTH_COMB_MIN_AREA &&
            component->contact_groups >=
                WATER_COAST_SMOOTH_COMB_MIN_CONTACT_GROUPS &&
            component->area * 100 <=
                box_area * WATER_COAST_SMOOTH_COMB_MAX_FILL_PERCENT;
        if (component->sparse_mesh) component->preserve = 0;
    }
}

static int collect_components(
    const unsigned char *ocean, const unsigned char *opened,
    int width, int height, int *component_ids, int *queue,
    unsigned char *contact_mask, int *contact_queue,
    ThinComponent **components, int *component_count,
    int *component_capacity,
    RenderWaterCoastSmoothingMetrics *metrics) {
    static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    int count = width * height;
    int start;
    for (start = 0; start < count; start++) {
        component_ids[start] = -1;
        contact_mask[start] = 0;
    }
    for (start = 0; start < count; start++) {
        ThinComponent component;
        int head = 0;
        int tail = 0;
        int id;
        if (!ocean[start] || opened[start] || component_ids[start] >= 0)
            continue;
        memset(&component, 0, sizeof(component));
        component.min_x = component.max_x = start % width;
        component.min_y = component.max_y = start / width;
        id = *component_count;
        component_ids[start] = id;
        queue[tail++] = start;
        while (head < tail) {
            int index = queue[head++];
            int x = index % width;
            int y = index / width;
            int n;
            int touches_opened = 0;
            component.area++;
            component.sum_x += x;
            component.sum_y += y;
            if (x < component.min_x) component.min_x = x;
            if (x > component.max_x) component.max_x = x;
            if (y < component.min_y) component.min_y = y;
            if (y > component.max_y) component.max_y = y;
            for (n = 0; n < 8; n++) {
                int nx = x + dx[n];
                int ny = y + dy[n];
                int neighbor;
                if (nx < 0 || ny < 0 || nx >= width || ny >= height)
                    continue;
                neighbor = ny * width + nx;
                if (opened[neighbor]) {
                    touches_opened = 1;
                    component.attachment_mask |= 1 << n;
                    component.contact_x += nx;
                    component.contact_y += ny;
                    component.contact_count++;
                } else if (ocean[neighbor]) {
                    component.cardinal_edges += (n & 1) == 0;
                    if (component_ids[neighbor] < 0) {
                        component_ids[neighbor] = id;
                        queue[tail++] = neighbor;
                    }
                }
            }
            contact_mask[index] = (unsigned char)touches_opened;
        }
        {
            int q;
            for (q = 0; q < tail; q++) {
                int contact_head = 0;
                int contact_tail = 0;
                int seed = queue[q];
                if (contact_mask[seed] != 1) continue;
                component.contact_groups++;
                contact_mask[seed] = 2;
                contact_queue[contact_tail++] = seed;
                while (contact_head < contact_tail) {
                    int index = contact_queue[contact_head++];
                    int x = index % width;
                    int y = index / width;
                    int n;
                    for (n = 0; n < 8; n++) {
                        int nx = x + dx[n];
                        int ny = y + dy[n];
                        int neighbor;
                        if (nx < 0 || ny < 0 || nx >= width || ny >= height)
                            continue;
                        neighbor = ny * width + nx;
                        if (component_ids[neighbor] != id ||
                            contact_mask[neighbor] != 1) continue;
                        contact_mask[neighbor] = 2;
                        contact_queue[contact_tail++] = neighbor;
                    }
                }
            }
        }
        component.cardinal_edges /= 2;
        classify_component(&component);
        metrics->thin_components++;
        if (component.preserve) metrics->preserved_components++;
        else metrics->one_ended_components++;
        if (component.sparse_mesh) {
            metrics->sparse_mesh_components++;
            metrics->sparse_mesh_tiles += (uint64_t)component.area;
        }
        if (!append_component(components, component_count, component_capacity,
                              component)) return 0;
    }
    return 1;
}

static int mark_clusters(ThinComponent *components, int count,
                         int width, int height,
                         RenderWaterCoastSmoothingMetrics *metrics) {
    int bucket_size = WATER_COAST_SMOOTH_CLUSTER_RADIUS + 1;
    int bucket_w = (width + bucket_size - 1) / bucket_size;
    int bucket_h = (height + bucket_size - 1) / bucket_size;
    size_t head_count = (size_t)bucket_w * (size_t)bucket_h * 4u;
    int *heads = (int *)malloc(head_count * sizeof(*heads));
    int *next = (int *)malloc((size_t)(count ? count : 1) * sizeof(*next));
    int i;
    if (!heads || !next) {
        free(next);
        free(heads);
        return 0;
    }
    for (i = 0; i < (int)head_count; i++) heads[i] = -1;
    for (i = 0; i < count; i++) {
        int bx;
        int by;
        int orientation;
        int bucket;
        next[i] = -1;
        if (components[i].preserve && !components[i].passage) continue;
        bx = components[i].anchor_x / bucket_size;
        by = components[i].anchor_y / bucket_size;
        orientation = components[i].direction & 3;
        bucket = (by * bucket_w + bx) * 4 + orientation;
        next[i] = heads[bucket];
        heads[bucket] = i;
    }
    for (i = 0; i < count; i++) {
        int nearby = 1;
        int bx;
        int by;
        int oy;
        int ox;
        int orientation_delta;
        if (components[i].preserve && !components[i].passage) continue;
        bx = components[i].anchor_x / bucket_size;
        by = components[i].anchor_y / bucket_size;
        for (oy = -1; oy <= 1 && nearby < WATER_COAST_SMOOTH_MIN_CLUSTER;
             oy++) {
            int sy = by + oy;
            if (sy < 0 || sy >= bucket_h) continue;
            for (ox = -1;
                 ox <= 1 && nearby < WATER_COAST_SMOOTH_MIN_CLUSTER; ox++) {
                int sx = bx + ox;
                if (sx < 0 || sx >= bucket_w) continue;
                for (orientation_delta = -1;
                     orientation_delta <= 1 &&
                     nearby < WATER_COAST_SMOOTH_MIN_CLUSTER;
                     orientation_delta++) {
                    int orientation =
                        (components[i].direction + orientation_delta + 8) & 3;
                    int bucket = (sy * bucket_w + sx) * 4 + orientation;
                    int j;
                    for (j = heads[bucket]; j >= 0; j = next[j]) {
                        int dx;
                        int dy;
                        if (i == j) continue;
                        metrics->candidate_comparisons++;
                        dx = abs(components[i].anchor_x -
                                 components[j].anchor_x);
                        dy = abs(components[i].anchor_y -
                                 components[j].anchor_y);
                        if ((dx > dy ? dx : dy) >
                                WATER_COAST_SMOOTH_CLUSTER_RADIUS ||
                            !sector_parallel(components[i].direction,
                                             components[j].direction) ||
                            !compatible_size(&components[i], &components[j]))
                            continue;
                        nearby++;
                        if (nearby >= WATER_COAST_SMOOTH_MIN_CLUSTER) break;
                    }
                }
            }
        }
        components[i].clustered = components[i].sparse_mesh ||
            nearby >= WATER_COAST_SMOOTH_MIN_CLUSTER;
    }
    metrics->transient_bytes += head_count * sizeof(*heads) +
        (uint64_t)(count ? count : 1) * sizeof(*next);
    free(next);
    free(heads);
    return 1;
}

static void accumulate_metrics(RenderWaterCoastSmoothingMetrics *total,
                               const RenderWaterCoastSmoothingMetrics *pass) {
    total->thin_components += pass->thin_components;
    total->one_ended_components += pass->one_ended_components;
    total->preserved_components += pass->preserved_components;
    total->sparse_mesh_components += pass->sparse_mesh_components;
    total->sparse_mesh_tiles += pass->sparse_mesh_tiles;
    total->regularized_components += pass->regularized_components;
    total->regularized_tiles += pass->regularized_tiles;
    total->candidate_comparisons += pass->candidate_comparisons;
    if (pass->transient_bytes > total->transient_bytes)
        total->transient_bytes = pass->transient_bytes;
}

int render_water_coast_smoothing_build(
    const unsigned char *ocean_mask, int width, int height,
    unsigned char *suppressed_tiles,
    RenderWaterCoastSmoothingMetrics *metrics) {
    unsigned char *working = NULL;
    unsigned char *core = NULL;
    unsigned char *opened = NULL;
    int *component_ids = NULL;
    int *queue = NULL;
    unsigned char *contact_mask = NULL;
    int *contact_queue = NULL;
    size_t count;
    int pass_index;
    int radius;
    if (!ocean_mask || !suppressed_tiles || !metrics ||
        width <= 0 || height <= 0) return 0;
    if ((size_t)width > SIZE_MAX / (size_t)height) return 0;
    count = (size_t)width * (size_t)height;
    if (count > INT_MAX) return 0;
    memset(metrics, 0, sizeof(*metrics));
    memset(suppressed_tiles, 0, count);
    working = (unsigned char *)malloc(count);
    core = (unsigned char *)calloc(count, 1);
    opened = (unsigned char *)calloc(count, 1);
    component_ids = (int *)malloc(count * sizeof(*component_ids));
    queue = (int *)malloc(count * sizeof(*queue));
    contact_mask = (unsigned char *)malloc(count);
    contact_queue = (int *)malloc(count * sizeof(*contact_queue));
    if (!working || !core || !opened || !component_ids || !queue ||
        !contact_mask || !contact_queue)
        goto failure;
    for (pass_index = 0; pass_index <= WATER_COAST_SMOOTH_MAX_PASSES;
         pass_index++) {
        uint64_t before = metrics->regularized_tiles;
        for (radius = WATER_COAST_SMOOTH_RADIUS;
             radius <= WATER_COAST_SMOOTH_MAX_RADIUS; radius++) {
            RenderWaterCoastSmoothingMetrics pass = {0};
            ThinComponent *components = NULL;
            int component_count = 0;
            int component_capacity = 0;
            int i;
            for (i = 0; i < (int)count; i++)
                working[i] = (unsigned char)(ocean_mask[i] &&
                                             !suppressed_tiles[i]);
            pass.transient_bytes = count * 16u;
            build_opened_mask(working, width, height, radius, core, opened);
            if (!collect_components(working, opened, width, height,
                                    component_ids, queue,
                                    contact_mask, contact_queue, &components,
                                    &component_count, &component_capacity,
                                    &pass) ||
                !mark_clusters(components, component_count, width, height,
                               &pass)) {
                free(components);
                goto failure;
            }
            for (i = 0; i < (int)count; i++) {
                int id = component_ids[i];
                if (id < 0 || id >= component_count ||
                    !components[id].clustered || suppressed_tiles[i])
                    continue;
                suppressed_tiles[i] = 1;
                pass.regularized_tiles++;
            }
            for (i = 0; i < component_count; i++)
                pass.regularized_components += components[i].clustered;
            pass.transient_bytes +=
                (uint64_t)component_capacity * sizeof(*components);
            accumulate_metrics(metrics, &pass);
            free(components);
        }
        metrics->pass_count = pass_index + 1;
        if (metrics->regularized_tiles == before) {
            metrics->converged = 1;
            break;
        }
        if (pass_index == WATER_COAST_SMOOTH_MAX_PASSES) goto failure;
    }
    free(contact_queue);
    free(contact_mask);
    free(queue);
    free(component_ids);
    free(opened);
    free(core);
    free(working);
    return 1;
failure:
    free(contact_queue);
    free(contact_mask);
    free(queue);
    free(component_ids);
    free(opened);
    free(core);
    free(working);
    return 0;
}
