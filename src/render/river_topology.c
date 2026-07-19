#include "render/river_topology.h"
#include "world/river_path_validation.h"
#include "world/terrain_query.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define ENDPOINT_BUCKET_COUNT 32768

static RiverTopologyPathLink *path_links;
static RiverTopologyStem *stems;
static int endpoint_heads[ENDPOINT_BUCKET_COUNT];
static int *endpoint_next;
static int *parents;
static int *root_stems;
static int storage_count;
static int stem_storage_count;
static RiverTopologyView latest_view;

static int point_tile_index(const RenderSnapshot *snapshot,
                            RiverRenderPoint point) {
    int x = (point.x10 - 5) / 10;
    int y = (point.y10 - 5) / 10;
    if (!snapshot || x < 0 || y < 0 ||
        x >= snapshot->map_w || y >= snapshot->map_h) return -1;
    return y * snapshot->map_w + x;
}

static int lake_tile(const RenderSnapshot *snapshot, int index) {
    return snapshot && index >= 0 &&
           index < snapshot->map_w * snapshot->map_h &&
           (Geography)snapshot->tiles[index].geography == GEO_LAKE;
}

static int label_lake_components(const RenderSnapshot *snapshot,
                                 int *labels, int *queue) {
    static const int dx[4] = {1, 0, -1, 0};
    static const int dy[4] = {0, 1, 0, -1};
    int tile_count = snapshot->map_w * snapshot->map_h;
    int component_count = 0;
    int index;
    memset(labels, 0xff, (size_t)tile_count * sizeof(*labels));
    for (index = 0; index < tile_count; index++) {
        int head = 0, tail = 0;
        if (!lake_tile(snapshot, index) || labels[index] >= 0) continue;
        labels[index] = component_count;
        queue[tail++] = index;
        while (head < tail) {
            int current = queue[head++];
            int x = current % snapshot->map_w;
            int y = current / snapshot->map_w;
            int direction;
            for (direction = 0; direction < 4; direction++) {
                int nx = x + dx[direction];
                int ny = y + dy[direction];
                int neighbor;
                if (nx < 0 || ny < 0 || nx >= snapshot->map_w ||
                    ny >= snapshot->map_h) continue;
                neighbor = ny * snapshot->map_w + nx;
                if (!lake_tile(snapshot, neighbor) || labels[neighbor] >= 0)
                    continue;
                labels[neighbor] = component_count;
                queue[tail++] = neighbor;
            }
        }
        component_count++;
    }
    return component_count;
}

static int resize_storage(int count) {
    RiverTopologyPathLink *new_links;
    int *new_next;
    int *new_parents;
    int *new_roots;
    if (count == storage_count && (count == 0 || path_links)) return 1;
    if (!river_path_count_valid(count, MAX_MAP_W, MAX_MAP_H)) return 0;
    if (count == 0) {
        river_topology_release();
        return 1;
    }
    new_links = (RiverTopologyPathLink *)calloc((size_t)count, sizeof(*new_links));
    new_next = (int *)malloc((size_t)count * sizeof(*new_next));
    new_parents = (int *)malloc((size_t)count * sizeof(*new_parents));
    new_roots = (int *)malloc((size_t)count * sizeof(*new_roots));
    if (!new_links || !new_next || !new_parents || !new_roots) {
        free(new_links); free(new_next);
        free(new_parents); free(new_roots);
        return 0;
    }
    free(path_links); free(endpoint_next);
    free(parents); free(root_stems);
    path_links = new_links; endpoint_next = new_next;
    parents = new_parents; root_stems = new_roots; storage_count = count;
    return 1;
}

static int resize_stems(int count) {
    RiverTopologyStem *new_stems;
    if (count <= 0 || count > storage_count) return 0;
    if (count == stem_storage_count && stems) return 1;
    new_stems = (RiverTopologyStem *)calloc((size_t)count, sizeof(*new_stems));
    if (!new_stems) return 0;
    free(stems);
    stems = new_stems;
    stem_storage_count = count;
    return 1;
}

static unsigned endpoint_hash(int x10, int y10) {
    unsigned value = (unsigned)x10 * 73856093u ^ (unsigned)y10 * 19349663u;
    return value & (ENDPOINT_BUCKET_COUNT - 1u);
}

static int is_distributary(const RiverRenderPath *path) {
    return path && (path->semantic_flags & SNAPSHOT_RIVER_DISTRIBUTARY) != 0;
}

static int better_downstream(const RiverRenderPath *paths, int candidate, int current) {
    const RiverRenderPath *left;
    const RiverRenderPath *right;
    if (current < 0) return 1;
    left = &paths[candidate];
    right = &paths[current];
    if (is_distributary(left) != is_distributary(right))
        return !is_distributary(left);
    if (left->order != right->order) return left->order > right->order;
    if (left->flow != right->flow) return left->flow > right->flow;
    if (left->raw_point_count != right->raw_point_count)
        return left->raw_point_count > right->raw_point_count;
    return candidate < current;
}

static int better_upstream(const RiverRenderPath *paths, int candidate, int current) {
    const RiverRenderPath *left;
    const RiverRenderPath *right;
    if (current < 0) return 1;
    left = &paths[candidate];
    right = &paths[current];
    if (left->terminal_inflow != right->terminal_inflow)
        return left->terminal_inflow > right->terminal_inflow;
    if (left->order != right->order) return left->order > right->order;
    if (left->flow != right->flow) return left->flow > right->flow;
    if (left->raw_point_count != right->raw_point_count)
        return left->raw_point_count > right->raw_point_count;
    return candidate < current;
}

static int find_root(int value) {
    int root = value;
    while (parents[root] != root) root = parents[root];
    while (parents[value] != value) {
        int next = parents[value];
        parents[value] = root;
        value = next;
    }
    return root;
}

static void join(int left, int right) {
    int left_root = find_root(left);
    int right_root = find_root(right);
    if (left_root == right_root) return;
    if (left_root < right_root) parents[right_root] = left_root;
    else parents[left_root] = right_root;
}

static void build_endpoint_index(const RiverRenderPath *paths, int count) {
    int i;
    memset(endpoint_heads, 0xff, sizeof(endpoint_heads));
    for (i = 0; i < count; i++) {
        unsigned bucket = endpoint_hash(paths[i].points[0].x10, paths[i].points[0].y10);
        endpoint_next[i] = endpoint_heads[bucket];
        endpoint_heads[bucket] = i;
    }
}

static int downstream_at_endpoint(const RiverRenderPath *paths, int count, int path_index) {
    const RiverRenderPath *path = &paths[path_index];
    RiverRenderPoint end = path->points[path->point_count - 1];
    unsigned bucket = endpoint_hash(end.x10, end.y10);
    int best = -1;
    int candidate;
    (void)count;
    for (candidate = endpoint_heads[bucket]; candidate >= 0;
         candidate = endpoint_next[candidate]) {
        const RiverRenderPath *next = &paths[candidate];
        if (candidate == path_index || next->points[0].x10 != end.x10 ||
            next->points[0].y10 != end.y10) continue;
        if (better_downstream(paths, candidate, best)) best = candidate;
    }
    return best;
}

static int path_requires_continuation(const RenderSnapshot *snapshot,
                                      const RiverRenderPath *path) {
    int terminal;
    Geography geography;
    if (!snapshot || !path || path->point_count < 2 || is_distributary(path)) return 0;
    if (path->end_flags & (SNAPSHOT_RIVER_MOUTH |
                           SNAPSHOT_RIVER_CLOSED_BASIN |
                           SNAPSHOT_RIVER_SALT_LAKE)) return 0;
    if (path->end_flags & (SNAPSHOT_RIVER_LAKE |
                           SNAPSHOT_RIVER_DELTA |
                           SNAPSHOT_RIVER_CONFLUENCE)) return 1;
    terminal = point_tile_index(snapshot, path->points[path->point_count - 1]);
    if (terminal < 0) return 1;
    geography = (Geography)snapshot->tiles[terminal].geography;
    return is_land(geography);
}

static int connect_lake_outlets(const RenderSnapshot *snapshot,
                                const RiverRenderPath *paths, int count) {
    int tile_count;
    int *labels;
    int *queue;
    int *outlets;
    int component_count;
    int path;
    if (!snapshot || snapshot->map_w <= 0 ||
        snapshot->map_h <= 0) return 0;
    tile_count = snapshot->map_w * snapshot->map_h;
    labels = (int *)malloc((size_t)tile_count * sizeof(*labels));
    queue = (int *)malloc((size_t)tile_count * sizeof(*queue));
    if (!labels || !queue) {
        free(labels); free(queue);
        return 0;
    }
    component_count = label_lake_components(snapshot, labels, queue);
    free(queue);
    if (component_count <= 0) {
        free(labels);
        return 1;
    }
    outlets = (int *)malloc((size_t)component_count * sizeof(*outlets));
    if (!outlets) {
        free(labels);
        return 0;
    }
    memset(outlets, 0xff, (size_t)component_count * sizeof(*outlets));
    for (path = 0; path < count; path++) {
        int first;
        int second;
        int component;
        if (paths[path].point_count < 2 || is_distributary(&paths[path])) continue;
        first = point_tile_index(snapshot, paths[path].points[0]);
        second = point_tile_index(snapshot, paths[path].points[1]);
        if (!lake_tile(snapshot, first) || lake_tile(snapshot, second)) continue;
        component = labels[first];
        if (component >= 0 && component < component_count &&
            better_downstream(paths, path, outlets[component])) {
            outlets[component] = path;
        }
    }
    for (path = 0; path < count; path++) {
        int terminal;
        int component;
        int outlet;
        if (path_links[path].downstream_path >= 0 ||
            !(paths[path].end_flags & SNAPSHOT_RIVER_LAKE) ||
            (paths[path].end_flags & (SNAPSHOT_RIVER_CLOSED_BASIN |
                                      SNAPSHOT_RIVER_SALT_LAKE))) continue;
        terminal = point_tile_index(
            snapshot, paths[path].points[paths[path].point_count - 1]);
        component = terminal >= 0 ? labels[terminal] : -1;
        outlet = component >= 0 && component < component_count ?
            outlets[component] : -1;
        if (outlet >= 0 && outlet != path)
            path_links[path].downstream_path = outlet;
    }
    free(outlets);
    free(labels);
    return 1;
}

static void join_delta_branches(const RiverRenderPath *paths, int count) {
    int path;
    for (path = 0; path < count; path++) {
        RiverRenderPoint end;
        unsigned bucket;
        int candidate;
        if (is_distributary(&paths[path]) ||
            !(paths[path].end_flags & SNAPSHOT_RIVER_DELTA)) continue;
        end = paths[path].points[paths[path].point_count - 1];
        bucket = endpoint_hash(end.x10, end.y10);
        for (candidate = endpoint_heads[bucket]; candidate >= 0;
             candidate = endpoint_next[candidate]) {
            const RiverRenderPath *branch = &paths[candidate];
            if (!is_distributary(branch) || branch->points[0].x10 != end.x10 ||
                branch->points[0].y10 != end.y10) continue;
            join(path, candidate);
        }
    }
}

static int build_links(const RenderSnapshot *snapshot,
                       const RiverRenderPath *paths, int count) {
    int i;
    for (i = 0; i < count; i++) {
        path_links[i].downstream_path = downstream_at_endpoint(paths, count, i);
        path_links[i].dominant_upstream_path = -1;
        path_links[i].stem_id = -1;
        path_links[i].continuation_required =
            (unsigned char)path_requires_continuation(snapshot, &paths[i]);
        parents[i] = i;
    }
    if (!connect_lake_outlets(snapshot, paths, count)) return 0;
    for (i = 0; i < count; i++) {
        int downstream = path_links[i].downstream_path;
        if (downstream >= 0 && better_upstream(
                paths, i, path_links[downstream].dominant_upstream_path)) {
            path_links[downstream].dominant_upstream_path = i;
        }
    }
    for (i = 0; i < count; i++) {
        int downstream = path_links[i].downstream_path;
        if (downstream >= 0 && path_links[downstream].dominant_upstream_path == i)
            join(i, downstream);
    }
    join_delta_branches(paths, count);
    return 1;
}

static int assign_stem_roots(int count) {
    int stem_count = 0;
    int i;
    memset(root_stems, 0xff, (size_t)count * sizeof(*root_stems));
    for (i = 0; i < count; i++) {
        int root = find_root(i);
        if (root_stems[root] < 0) root_stems[root] = stem_count++;
    }
    return stem_count;
}

static void collect_stems(const RiverRenderPath *paths, int count,
                          int stem_count) {
    int i;
    memset(stems, 0, (size_t)stem_count * sizeof(*stems));
    for (i = 0; i < stem_count; i++) {
        stems[i].downstream_stem = -1;
        stems[i].stable_path = INT_MAX;
    }
    for (i = 0; i < count; i++) {
        const RiverRenderPath *path = &paths[i];
        int stem = root_stems[find_root(i)];
        RiverTopologyStem *out = &stems[stem];
        path_links[i].stem_id = stem;
        out->path_count++;
        out->raw_length += path->raw_point_count;
        if (path->order > out->max_order) out->max_order = path->order;
        if ((uint32_t)path->flow > out->max_flow) out->max_flow = (uint32_t)path->flow;
        if ((uint32_t)path->terminal_inflow > out->max_terminal_inflow)
            out->max_terminal_inflow = (uint32_t)path->terminal_inflow;
        if (i < out->stable_path) out->stable_path = i;
        if (path->end_flags & (SNAPSHOT_RIVER_MOUTH | SNAPSHOT_RIVER_DELTA |
                               SNAPSHOT_RIVER_CLOSED_BASIN |
                               SNAPSHOT_RIVER_SALT_LAKE)) {
            out->has_outlet = 1;
        }
    }
    for (i = 0; i < count; i++) {
        int downstream = path_links[i].downstream_path;
        int stem = path_links[i].stem_id;
        int downstream_stem = downstream >= 0 ? path_links[downstream].stem_id : -1;
        if (downstream_stem >= 0 && downstream_stem != stem) {
            int current = stems[stem].downstream_stem;
            if (current < 0 || stems[downstream_stem].max_flow > stems[current].max_flow ||
                (stems[downstream_stem].max_flow == stems[current].max_flow &&
                 downstream_stem < current)) stems[stem].downstream_stem = downstream_stem;
        }
    }
    latest_view.stem_count = stem_count;
}

int river_topology_rebuild(const RenderSnapshot *snapshot,
                           const RiverRenderPath *paths, int count, int revision) {
    int stem_count;
    memset(&latest_view, 0, sizeof(latest_view));
    if (!paths || count <= 0 || !resize_storage(count)) return 0;
    build_endpoint_index(paths, count);
    if (!build_links(snapshot, paths, count)) return 0;
    stem_count = assign_stem_roots(count);
    if (!resize_stems(stem_count)) return 0;
    collect_stems(paths, count, stem_count);
    latest_view.revision = revision;
    latest_view.path_count = count;
    latest_view.paths = path_links;
    latest_view.stems = stems;
    return 1;
}

const RiverTopologyView *river_topology_view(void) { return &latest_view; }

void river_topology_release(void) {
    free(path_links); free(stems); free(endpoint_next);
    free(parents); free(root_stems);
    path_links = NULL; stems = NULL; endpoint_next = NULL;
    parents = NULL; root_stems = NULL;
    storage_count = stem_storage_count = 0;
    memset(&latest_view, 0, sizeof(latest_view));
}

size_t river_topology_retained_bytes(void) {
    return (size_t)storage_count *
               (sizeof(*path_links) + sizeof(*endpoint_next) +
                sizeof(*parents) + sizeof(*root_stems)) +
           (size_t)stem_storage_count * sizeof(*stems);
}
