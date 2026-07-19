#include "render/river_lod_policy.h"
#include "render/river_presentation_filter.h"
#include "world/river_path_validation.h"

#include <stdlib.h>
#include <string.h>

static unsigned char *masks;
static unsigned char *selected_stems;
static unsigned char *visible_stems;
static int *stem_order;
static RiverLodPolicyMetrics metrics[4];
static const RiverTopologyView *sort_topology;
static int prepared_path_count;
static int prepared_revision;
static int prepared_w;
static int prepared_h;
static int prepared_stem_count;
static const RiverRenderPath *prepared_paths;

static unsigned char *lod_mask(int lod) {
    return masks + (size_t)lod * (size_t)prepared_path_count;
}

static int resize_storage(int count, int stem_count, int map_w, int map_h) {
    unsigned char *new_masks;
    unsigned char *new_selected;
    unsigned char *new_visible;
    int *new_order;
    if (!river_path_count_valid(count, map_w, map_h) ||
        stem_count <= 0 || stem_count > count) return 0;
    if (count == prepared_path_count && stem_count == prepared_stem_count && masks)
        return 1;
    new_masks = (unsigned char *)calloc((size_t)count * 4u, 1);
    new_selected = (unsigned char *)calloc((size_t)stem_count, 1);
    new_visible = (unsigned char *)calloc((size_t)stem_count, 1);
    new_order = (int *)malloc((size_t)stem_count * sizeof(*new_order));
    if (!new_masks || !new_selected || !new_visible || !new_order) {
        free(new_masks); free(new_selected); free(new_visible); free(new_order);
        return 0;
    }
    free(masks); free(selected_stems); free(visible_stems); free(stem_order);
    masks = new_masks; selected_stems = new_selected;
    visible_stems = new_visible; stem_order = new_order;
    prepared_path_count = count;
    prepared_stem_count = stem_count;
    return 1;
}

static int compare_stems(const void *left_value, const void *right_value) {
    int left_index = *(const int *)left_value;
    int right_index = *(const int *)right_value;
    const RiverTopologyStem *left = &sort_topology->stems[left_index];
    const RiverTopologyStem *right = &sort_topology->stems[right_index];
    if (left->max_order != right->max_order)
        return left->max_order > right->max_order ? -1 : 1;
    if (left->max_terminal_inflow != right->max_terminal_inflow)
        return left->max_terminal_inflow > right->max_terminal_inflow ? -1 : 1;
    if (left->max_flow != right->max_flow)
        return left->max_flow > right->max_flow ? -1 : 1;
    if (left->has_outlet != right->has_outlet) return left->has_outlet ? -1 : 1;
    if (left->raw_length != right->raw_length)
        return left->raw_length > right->raw_length ? -1 : 1;
    return left->stable_path < right->stable_path ? -1 :
           left->stable_path > right->stable_path;
}

static int percentage_target(int path_count, int percent) {
    int target = (path_count * percent + 99) / 100;
    return target > 0 ? target : 1;
}

static int selected_path_count(const RiverTopologyView *topology) {
    int count = 0;
    int stem;
    for (stem = 0; stem < topology->stem_count; stem++) {
        if (selected_stems[stem]) count += topology->stems[stem].path_count;
    }
    return count;
}

static int build_prefix_closure(const RiverTopologyView *topology, int prefix) {
    int position;
    memset(selected_stems, 0, (size_t)topology->stem_count);
    for (position = 0; position < prefix; position++) {
        int stem = stem_order[position];
        int guard = 0;
        while (stem >= 0 && stem < topology->stem_count &&
               !selected_stems[stem] && guard++ < topology->stem_count) {
            selected_stems[stem] = 1;
            stem = topology->stems[stem].downstream_stem;
        }
    }
    return selected_path_count(topology);
}

static int select_prefix_for_budget(const RiverTopologyView *topology,
                                    int minimum_prefix, int minimum,
                                    int target, int maximum) {
    int low = minimum_prefix;
    int high = topology->stem_count;
    int best = minimum_prefix;
    int selected;
    while (low <= high) {
        int middle = low + (high - low) / 2;
        int count = build_prefix_closure(topology, middle);
        if (count <= target) {
            best = middle;
            low = middle + 1;
        } else {
            high = middle - 1;
        }
    }
    selected = build_prefix_closure(topology, best);
    if (selected < minimum && best < topology->stem_count) {
        int corrected = build_prefix_closure(topology, best + 1);
        int selected_distance = target - selected;
        int corrected_distance = corrected - target;
        if (selected_distance < 0) selected_distance = -selected_distance;
        if (corrected_distance < 0) corrected_distance = -corrected_distance;
        if (corrected <= maximum || corrected_distance <= selected_distance ||
            best == 0) {
            best++;
        } else {
            build_prefix_closure(topology, best);
        }
    }
    return best;
}

static void summarize_mask(const RiverTopologyView *topology, int lod, int target) {
    RiverLodPolicyMetrics *out = &metrics[lod];
    int path;
    memset(out, 0, sizeof(*out));
    memset(visible_stems, 0, (size_t)topology->stem_count);
    out->target_paths = target;
    for (path = 0; path < topology->path_count; path++) {
        int stem = topology->paths[path].stem_id;
        if (stem >= 0 && stem < topology->stem_count && lod_mask(lod)[path]) {
            int downstream = topology->paths[path].downstream_path;
            visible_stems[stem] = 1;
            out->visible_paths++;
            if ((downstream < 0 && !topology->paths[path].continuation_required) ||
                (downstream >= 0 && downstream < topology->path_count &&
                 lod_mask(lod)[downstream])) {
                out->connected_paths++;
            }
        }
    }
    for (path = 0; path < topology->stem_count; path++)
        out->visible_stems += visible_stems[path] != 0;
}

static void capture_mask(const RiverTopologyView *topology, int lod, int target) {
    int path;
    memset(lod_mask(lod), 0, (size_t)topology->path_count);
    for (path = 0; path < topology->path_count; path++) {
        int stem = topology->paths[path].stem_id;
        if (stem >= 0 && stem < topology->stem_count && selected_stems[stem])
            lod_mask(lod)[path] = 1;
    }
    summarize_mask(topology, lod, target);
}

static void capture_close_mask(const RiverRenderPath *paths, int count,
                               const RiverTopologyView *topology, int target) {
    int path;
    memset(lod_mask(3), 0, (size_t)count);
    for (path = 0; path < count; path++)
        lod_mask(3)[path] = paths[path].active && paths[path].point_count >= 2;
    summarize_mask(topology, 3, target);
}

static void note_close_filter_metrics(RiverLodPolicyMetrics *out,
                                      RiverPresentationFilterMetrics filter) {
    out->close_candidate_stems = filter.candidate_stems;
    out->close_kept_candidate_stems = filter.kept_candidate_stems;
    out->close_suppressed_stems = filter.suppressed_stems;
    out->close_protected_paths = filter.protected_semantic_paths;
    out->close_protected_hidden_paths = filter.protected_hidden_paths;
    out->close_parallel_conflicts_before = filter.parallel_conflicts_before;
    out->close_parallel_conflicts_after = filter.parallel_conflicts_after;
}

int river_lod_policy_prepare(const RiverRenderPath *paths, int count,
                             const RiverTopologyView *topology,
                             int viewport_w, int viewport_h) {
    static const int minimum_percent[3] = {15, 35, 70};
    static const int target_percent[3] = {18, 40, 75};
    static const int maximum_percent[3] = {20, 45, 80};
    int targets[4];
    int prefix = 0;
    int stem;
    RiverPresentationFilterMetrics close_filter = {0};
    if (!topology || topology->path_count != count || count <= 0 ||
        topology->stem_count <= 0) {
        river_lod_policy_release();
        return 0;
    }
    if (prepared_paths == paths && prepared_revision == topology->revision &&
        prepared_path_count == count &&
        prepared_stem_count == topology->stem_count && prepared_w == viewport_w &&
        prepared_h == viewport_h) return 1;
    if (!resize_storage(count, topology->stem_count, viewport_w, viewport_h)) {
        river_lod_policy_release();
        return 0;
    }
    memset(masks, 0, (size_t)count * 4u);
    memset(metrics, 0, sizeof(metrics));
    river_presentation_filter_reset();
    targets[0] = percentage_target(count, target_percent[0]);
    targets[1] = percentage_target(count, target_percent[1]);
    targets[2] = percentage_target(count, target_percent[2]);
    targets[3] = count;
    sort_topology = topology;
    for (stem = 0; stem < topology->stem_count; stem++) stem_order[stem] = stem;
    qsort(stem_order, (size_t)topology->stem_count, sizeof(stem_order[0]), compare_stems);
    for (stem = 0; stem < 3; stem++) {
        prefix = select_prefix_for_budget(
            topology, prefix,
            percentage_target(count, minimum_percent[stem]),
            targets[stem],
            percentage_target(count, maximum_percent[stem]));
        capture_mask(topology, stem, targets[stem]);
    }
    capture_close_mask(paths, count, topology, targets[3]);
    river_presentation_filter_apply_close(
        paths, count, topology, lod_mask(3), &close_filter);
    summarize_mask(topology, 3, targets[3]);
    note_close_filter_metrics(&metrics[3], close_filter);
    prepared_revision = topology->revision;
    prepared_w = viewport_w;
    prepared_h = viewport_h;
    prepared_paths = paths;
    return 1;
}

int river_lod_policy_path_visible(int path_index, int lod) {
    if (path_index < 0 || lod < 0 || lod >= 4) return 0;
    if (!masks) return lod == 3;
    return path_index < prepared_path_count && lod_mask(lod)[path_index] != 0;
}

RiverLodPolicyMetrics river_lod_policy_metrics(int lod) {
    RiverLodPolicyMetrics empty = {0};
    return lod >= 0 && lod < 4 ? metrics[lod] : empty;
}

void river_lod_policy_release(void) {
    free(masks); free(selected_stems); free(visible_stems); free(stem_order);
    masks = NULL; selected_stems = NULL; visible_stems = NULL; stem_order = NULL;
    prepared_path_count = prepared_stem_count = 0;
    prepared_revision = prepared_w = prepared_h = 0;
    prepared_paths = NULL;
    memset(metrics, 0, sizeof(metrics));
    river_presentation_filter_reset();
}

size_t river_lod_policy_retained_bytes(void) {
    return (size_t)prepared_path_count * 4u +
           (size_t)prepared_stem_count * (2u + sizeof(*stem_order));
}
