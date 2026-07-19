#include "game/game_worldgen_lake_lifecycle_probe.h"
#include "game/game_worldgen_lake_lifecycle_render_probe.h"

#include "world/rivers.h"
#include "world/world_gen_classify.h"
#include "world/world_gen_context.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    LAKE_FIXTURE_W = 17,
    LAKE_FIXTURE_H = 17,
    LAKE_FIXTURE_COUNT = LAKE_FIXTURE_W * LAKE_FIXTURE_H,
    LAKE_LEFT = 7,
    LAKE_TOP = 7,
    LAKE_RIGHT = 9,
    LAKE_BOTTOM = 9,
    LAKE_EXPECTED_CELLS = 9
};

typedef struct {
    uint8_t lake[LAKE_FIXTURE_COUNT];
    int components;
    int cells;
    int closed_cells;
    int inlet_edges;
    int channel_inlets;
    int outlet_edges;
    int route_errors;
    int inlet;
    int inlet_receiver;
    int outlet;
    int outlet_receiver;
} LakeTopologyEvidence;

typedef struct {
    int inlet_segments;
    int outlet_segments;
    int internal_lake_segments;
    int invalid_segments;
} LakeSegmentEvidence;

typedef struct {
    int inlet_path;
    int outlet_path;
    int internal_lake_edges;
    int invalid_points;
} LakePathEvidence;

static const int CARDINAL_DX[4] = {1, 0, -1, 0};
static const int CARDINAL_DY[4] = {0, 1, 0, -1};

static int index_at(int x, int y) {
    return y * LAKE_FIXTURE_W + x;
}

static void set_height(WorldGenContext *context, int x, int y, int elevation) {
    int index = index_at(x, y);
    context->base_elevation[index] = (int16_t)elevation;
    context->elevation[index] = (int16_t)elevation;
}

static int create_fixture(WorldGenContext *context) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    int x;
    int y;
    config.seed = UINT32_C(0x0a6e1a4e);
    config.random_seed = 0;
    config.moisture = 100;
    config.bias_wetland = 100;
    if (!world_gen_context_create(context, &config, LAKE_FIXTURE_W,
                                  LAKE_FIXTURE_H, config.seed)) return 0;
    for (y = 0; y < LAKE_FIXTURE_H; y++) {
        for (x = 0; x < LAKE_FIXTURE_W; x++) {
            int index = index_at(x, y);
            int edge = x == 0 || y == 0 || x == LAKE_FIXTURE_W - 1 ||
                       y == LAKE_FIXTURE_H - 1;
            context->land_mask[index] = edge ? 0 : 1;
            set_height(context, x, y, edge ? 0 : 95);
            context->relative_altitude[index] = edge ? 0 : 12;
            context->slope[index] = 0;
            context->curvature[index] = 0;
            context->mountain_uplift[index] = 0;
            context->ocean_distance[index] = edge ? 0 : 8;
            context->moisture[index] = edge ? 100 : 82;
            context->temperature[index] = 52;
            context->precipitation[index] = 100;
            context->geography[index] = edge ? GEO_OCEAN : GEO_PLAIN;
            context->climate[index] = edge ? CLIMATE_OCEANIC : CLIMATE_CONTINENTAL;
            context->ecology[index] = edge ? ECO_NONE : ECO_GRASSLAND;
            context->resource[index] = RESOURCE_FEATURE_NONE;
        }
    }
    for (y = 4; y <= 12; y++) {
        for (x = 2; x <= 5; x++) {
            set_height(context, x, y,
                       24 + (5 - x) * 2 + abs(y - 8) * 3);
        }
    }
    set_height(context, 6, 8, 22);
    for (y = LAKE_TOP; y <= LAKE_BOTTOM; y++) {
        for (x = LAKE_LEFT; x <= LAKE_RIGHT; x++) {
            set_height(context, x, y, 10);
        }
    }
    set_height(context, 10, 8, 20);
    set_height(context, 11, 8, 17);
    set_height(context, 12, 8, 14);
    set_height(context, 13, 8, 11);
    set_height(context, 14, 8, 8);
    set_height(context, 15, 8, 5);
    return 1;
}

static void collect_components(const RiverNetworkView *view,
                               LakeTopologyEvidence *evidence) {
    uint8_t visited[LAKE_FIXTURE_COUNT] = {0};
    int queue[LAKE_FIXTURE_COUNT];
    int i;
    for (i = 0; i < view->tile_count; i++) {
        evidence->lake[i] = (view->cell_flags[i] & RIVER_CELL_LAKE) != 0;
        evidence->cells += evidence->lake[i] != 0;
    }
    for (i = 0; i < view->tile_count; i++) {
        int head = 0;
        int tail = 0;
        if (!evidence->lake[i] || visited[i]) continue;
        evidence->components++;
        visited[i] = 1;
        queue[tail++] = i;
        while (head < tail) {
            int current = queue[head++];
            int x = current % view->width;
            int y = current / view->width;
            int direction;
            for (direction = 0; direction < 4; direction++) {
                int nx = x + CARDINAL_DX[direction];
                int ny = y + CARDINAL_DY[direction];
                int neighbor;
                if (nx < 0 || nx >= view->width || ny < 0 || ny >= view->height) continue;
                neighbor = ny * view->width + nx;
                if (!evidence->lake[neighbor] || visited[neighbor]) continue;
                visited[neighbor] = 1;
                queue[tail++] = neighbor;
            }
        }
    }
}

static void collect_topology(const RiverNetworkView *view,
                             LakeTopologyEvidence *evidence) {
    uint32_t best_inlet_flow = 0;
    int i;
    memset(evidence, 0, sizeof(*evidence));
    evidence->inlet = -1;
    evidence->inlet_receiver = -1;
    evidence->outlet = -1;
    evidence->outlet_receiver = -1;
    collect_components(view, evidence);
    for (i = 0; i < view->tile_count; i++) {
        int receiver = view->receiver[i];
        if (evidence->lake[i]) {
            int current = i;
            int steps = 0;
            evidence->closed_cells +=
                (view->cell_flags[i] & RIVER_CELL_CLOSED_BASIN) != 0;
            if (receiver < 0 || receiver >= view->tile_count ||
                !evidence->lake[receiver]) {
                evidence->outlet_edges++;
                evidence->outlet = i;
                evidence->outlet_receiver = receiver;
            }
            while (current >= 0 && current < view->tile_count &&
                   evidence->lake[current] && steps <= evidence->cells) {
                current = view->receiver[current];
                steps++;
            }
            if (steps > evidence->cells || current < 0 ||
                current >= view->tile_count) evidence->route_errors++;
        } else if (receiver >= 0 && receiver < view->tile_count &&
                   evidence->lake[receiver]) {
            evidence->inlet_edges++;
            if (view->cell_flags[i] & RIVER_CELL_CHANNEL) {
                evidence->channel_inlets++;
                if (evidence->inlet < 0 || view->flow[i] > best_inlet_flow) {
                    evidence->inlet = i;
                    evidence->inlet_receiver = receiver;
                    best_inlet_flow = view->flow[i];
                }
            }
        }
    }
    if (evidence->outlet_receiver >= 0 &&
        evidence->outlet_receiver < view->tile_count) {
        for (i = 0; i < view->tile_count; i++) {
            int current;
            int steps = 0;
            if (!evidence->lake[i]) continue;
            current = i;
            while (current >= 0 && current < view->tile_count &&
                   evidence->lake[current] && steps <= evidence->cells) {
                current = view->receiver[current];
                steps++;
            }
            if (current != evidence->outlet_receiver) evidence->route_errors++;
        }
    }
}

static void collect_segments(const RiverNetworkView *view,
                             const LakeTopologyEvidence *topology,
                             LakeSegmentEvidence *evidence) {
    int i;
    memset(evidence, 0, sizeof(*evidence));
    for (i = 0; i < view->segment_count; i++) {
        const RiverNetworkSegment *segment = &view->segments[i];
        if (segment->from < 0 || segment->from >= view->tile_count ||
            segment->to < 0 || segment->to >= view->tile_count) {
            evidence->invalid_segments++;
            continue;
        }
        if (segment->kind != RIVER_SEGMENT_ORDINARY) continue;
        evidence->inlet_segments += segment->from == topology->inlet &&
            segment->to == topology->inlet_receiver;
        evidence->outlet_segments += segment->from == topology->outlet &&
            segment->to == topology->outlet_receiver;
        evidence->internal_lake_segments += topology->lake[segment->from] &&
            topology->lake[segment->to];
    }
}

static int path_point_index(const RiverPath *path, int point) {
    return path->points[point].y * LAKE_FIXTURE_W + path->points[point].x;
}

static void collect_paths(const RiverPath *paths, int count,
                          const LakeTopologyEvidence *topology,
                          LakePathEvidence *evidence) {
    int path_index;
    memset(evidence, 0, sizeof(*evidence));
    evidence->inlet_path = -1;
    evidence->outlet_path = -1;
    for (path_index = 0; path_index < count; path_index++) {
        const RiverPath *path = &paths[path_index];
        int point;
        if (!path->active || path->point_count < 2 ||
            path->point_count > MAX_RIVER_POINTS) {
            evidence->invalid_points++;
            continue;
        }
        for (point = 0; point + 1 < path->point_count; point++) {
            int from;
            int to;
            if (path->points[point].x < 0 ||
                path->points[point].x >= LAKE_FIXTURE_W ||
                path->points[point].y < 0 ||
                path->points[point].y >= LAKE_FIXTURE_H ||
                path->points[point + 1].x < 0 ||
                path->points[point + 1].x >= LAKE_FIXTURE_W ||
                path->points[point + 1].y < 0 ||
                path->points[point + 1].y >= LAKE_FIXTURE_H) {
                evidence->invalid_points++;
                continue;
            }
            from = path_point_index(path, point);
            to = path_point_index(path, point + 1);
            if (from < 0 || from >= LAKE_FIXTURE_COUNT ||
                to < 0 || to >= LAKE_FIXTURE_COUNT) {
                evidence->invalid_points++;
                continue;
            }
            if (from == topology->inlet && to == topology->inlet_receiver &&
                point + 1 == path->point_count - 1) evidence->inlet_path = path_index;
            if (from == topology->outlet && to == topology->outlet_receiver &&
                point == 0) evidence->outlet_path = path_index;
            evidence->internal_lake_edges += topology->lake[from] && topology->lake[to];
        }
    }
}

int game_worldgen_lake_lifecycle_probe_run(FILE *file) {
    WorldGenContext context;
    const RiverNetworkView *view = NULL;
    RiverPath *paths = NULL;
    LakeTopologyEvidence topology;
    LakeSegmentEvidence segments;
    LakePathEvidence path_evidence;
    int required_paths = 0;
    int copied_paths = 0;
    int classification_errors = 0;
    int semantic_anchors = 0;
    int created = 0;
    int hydrology_ok = 0;
    int classify_ok = 0;
    int qualification_ok = 0;
    int topology_ok = 0;
    int flow_ok = 0;
    int segment_ok = 0;
    int path_ok = 0;
    int render_ok = 0;
    int ok = 0;
    int i;
    if (!file) return 0;
    memset(&context, 0, sizeof(context));
    memset(&topology, 0, sizeof(topology));
    memset(&segments, 0, sizeof(segments));
    memset(&path_evidence, 0, sizeof(path_evidence));
    created = create_fixture(&context);
    if (!created) goto done;
    hydrology_ok = world_gen_hydrology_build(&context);
    view = river_network_latest_view();
    if (!hydrology_ok || !view || !river_network_view_matches_context(&context)) goto done;
    collect_topology(view, &topology);
    qualification_ok = topology.components == 1 &&
        topology.cells == LAKE_EXPECTED_CELLS &&
        view->diagnostics.lake_candidate_components == 1 &&
        view->diagnostics.lake_qualified_components == 1 &&
        view->diagnostics.lake_rejected_components == 0 &&
        view->diagnostics.lake_final_invalid_components == 0 &&
        view->diagnostics.lake_cells == LAKE_EXPECTED_CELLS;
    topology_ok = topology.closed_cells == 0 && topology.inlet_edges > 0 &&
        topology.channel_inlets > 0 && topology.outlet_edges == 1 &&
        topology.outlet_receiver >= 0 && topology.outlet_receiver < view->tile_count &&
        !topology.lake[topology.outlet_receiver] &&
        abs(topology.outlet % view->width - topology.outlet_receiver % view->width) <= 1 &&
        abs(topology.outlet / view->width - topology.outlet_receiver / view->width) <= 1 &&
        topology.route_errors == 0;
    flow_ok = topology.inlet >= 0 && topology.outlet >= 0 &&
        (view->cell_flags[topology.inlet] & RIVER_CELL_CHANNEL) != 0 &&
        (view->cell_flags[topology.outlet] & RIVER_CELL_CHANNEL) != 0 &&
        view->flow[topology.inlet_receiver] >= view->flow[topology.inlet] &&
        view->flow[topology.outlet] >= view->flow[topology.inlet_receiver] &&
        view->flow[topology.outlet] >= view->flow[topology.inlet] &&
        view->diagnostics.flow_conservation_errors == 0 &&
        view->diagnostics.invalid_receivers == 0 &&
        view->diagnostics.cycle_errors == 0;
    collect_segments(view, &topology, &segments);
    segment_ok = segments.inlet_segments == 1 && segments.outlet_segments == 1 &&
        segments.internal_lake_segments == 0 && segments.invalid_segments == 0;
    classify_ok = world_gen_classify_final(&context);
    if (classify_ok) {
        for (i = 0; i < context.tile_count; i++) {
            if (topology.lake[i] != (context.geography[i] == GEO_LAKE)) {
                classification_errors++;
            }
        }
    }
    required_paths = river_network_count_legacy_paths(&context);
    if (required_paths > 0) {
        paths = (RiverPath *)calloc((size_t)required_paths, sizeof(*paths));
    }
    if (paths) copied_paths = river_network_copy_legacy_paths(paths, required_paths);
    if (copied_paths == required_paths && copied_paths > 0) {
        collect_paths(paths, copied_paths, &topology, &path_evidence);
        path_ok = path_evidence.inlet_path >= 0 && path_evidence.outlet_path >= 0 &&
            path_evidence.internal_lake_edges == 0 && path_evidence.invalid_points == 0;
    }
    if (path_ok) {
        render_ok = game_worldgen_lake_lifecycle_render_check(
            &context, &paths[path_evidence.inlet_path],
            &paths[path_evidence.outlet_path], topology.inlet_receiver,
            topology.outlet, &semantic_anchors);
    }
    ok = created && hydrology_ok && qualification_ok && topology_ok && flow_ok &&
         segment_ok && classify_ok && classification_errors == 0 && path_ok && render_ok;
done:
    fprintf(file, "case=lake_lifecycle_qualification created=%d hydrology=%d "
                   "components=%d cells=%d candidates=%d qualified=%d rejected=%d "
                   "final_invalid=%d ok=%d\n",
            created, hydrology_ok, topology.components, topology.cells,
            view ? view->diagnostics.lake_candidate_components : -1,
            view ? view->diagnostics.lake_qualified_components : -1,
            view ? view->diagnostics.lake_rejected_components : -1,
            view ? view->diagnostics.lake_final_invalid_components : -1,
            qualification_ok);
    fprintf(file, "case=lake_lifecycle_topology open=%d inlets=%d channel_inlets=%d "
                  "outlets=%d route_errors=%d inlet=%d/%d outlet=%d/%d ok=%d\n",
            topology.closed_cells == 0, topology.inlet_edges, topology.channel_inlets,
            topology.outlet_edges, topology.route_errors, topology.inlet,
            topology.inlet_receiver, topology.outlet, topology.outlet_receiver,
            topology_ok);
    fprintf(file, "case=lake_lifecycle_flow inlet_flow=%u outlet_flow=%u "
                  "conservation=%d invalid=%d cycles=%d ok=%d\n",
            view && topology.inlet >= 0 ? view->flow[topology.inlet] : 0u,
            view && topology.outlet >= 0 ? view->flow[topology.outlet] : 0u,
            view ? view->diagnostics.flow_conservation_errors : -1,
            view ? view->diagnostics.invalid_receivers : -1,
            view ? view->diagnostics.cycle_errors : -1, flow_ok);
    fprintf(file, "case=lake_lifecycle_segments inlet=%d outlet=%d internal=%d "
                  "invalid=%d ok=%d\n",
            segments.inlet_segments, segments.outlet_segments,
            segments.internal_lake_segments, segments.invalid_segments, segment_ok);
    fprintf(file, "case=lake_lifecycle_paths required=%d copied=%d inlet_path=%d "
                  "outlet_path=%d internal=%d invalid=%d ok=%d\n",
            required_paths, copied_paths, path_evidence.inlet_path,
            path_evidence.outlet_path, path_evidence.internal_lake_edges,
            path_evidence.invalid_points, path_ok);
    fprintf(file, "case=lake_lifecycle_render semantic_anchors=%d exact=%d "
                  "classify=%d classification_errors=%d ok=%d\n",
            semantic_anchors, render_ok, classify_ok, classification_errors,
            render_ok && classify_ok && classification_errors == 0);
    fprintf(file, "case=lake_lifecycle_summary ok=%d\n", ok);
    free(paths);
    river_network_release_transient();
    world_gen_context_destroy(&context);
    return ok;
}
