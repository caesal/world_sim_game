#include "game_worldgen_hydrology_probe.h"
#include "game/game_worldgen_hydrology_direction_probe.h"
#include "game/game_worldgen_hydrology_invariant_probe.h"
#include "game/game_worldgen_lake_semantics_probe.h"

#include "core/constants.h"
#include "core/game_state.h"
#include "core/world_types.h"
#include "world/mountain_gen.h"
#include "world/river_path_validation.h"
#include "world/rivers.h"
#include "world/world_gen_classify.h"
#include "world/world_gen_climate.h"
#include "world/world_gen_context.h"
#include "world/world_gen_elevation.h"
#include "world/world_physical_state.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int cases;
    int sources;
    int confluences;
    int lakes;
    int closed_basins;
    int salt_lakes;
    int mouths;
    int deltas;
    int distributaries;
    int decay_checked;
    int commit_guard_checked;
    int commit_guard_ok;
} HydrologyCoverage;

static HydrologyCoverage coverage;

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static uint64_t guard_hash_mix(uint64_t hash, uint64_t value) {
    int byte_index;
    for (byte_index = 0; byte_index < 8; byte_index++) {
        hash ^= (uint8_t)(value >> (byte_index * 8));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t committed_world_hash(const WorldGenContext *context) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int x;
    int y;
    for (y = 0; y < context->height; y++) {
        for (x = 0; x < context->width; x++) {
            const Tile *tile = &world[y][x];
            hash = guard_hash_mix(hash, (uint32_t)tile->geography);
            hash = guard_hash_mix(hash, (uint32_t)tile->climate);
            hash = guard_hash_mix(hash, (uint32_t)tile->ecology);
            hash = guard_hash_mix(hash, (uint32_t)tile->resource);
            hash = guard_hash_mix(hash, (uint32_t)tile->owner);
            hash = guard_hash_mix(hash, (uint32_t)tile->province_id);
            hash = guard_hash_mix(hash, (uint32_t)tile->region_id);
            hash = guard_hash_mix(hash, (uint32_t)tile->elevation);
            hash = guard_hash_mix(hash, (uint32_t)tile->moisture);
            hash = guard_hash_mix(hash, (uint32_t)tile->temperature);
            hash = guard_hash_mix(hash, (uint32_t)tile->resource_variation);
            hash = guard_hash_mix(hash, (uint32_t)tile->river);
        }
    }
    return hash;
}

static uint64_t committed_paths_hash(void) {
    uint64_t hash = guard_hash_mix(UINT64_C(1469598103934665603),
                                   (uint32_t)river_path_count);
    int i;
    if (!river_paths_validate(river_paths, river_path_count, MAP_W, MAP_H)) return hash;
    for (i = 0; i < river_path_count; i++) {
        const RiverPath *path = &river_paths[i];
        int point_count = clamp_int(path->point_count, 0, MAX_RIVER_POINTS);
        int point;
        hash = guard_hash_mix(hash, (uint32_t)path->active);
        hash = guard_hash_mix(hash, (uint32_t)path->point_count);
        hash = guard_hash_mix(hash, (uint32_t)path->width);
        hash = guard_hash_mix(hash, (uint32_t)path->flow);
        hash = guard_hash_mix(hash, (uint32_t)path->order);
        for (point = 0; point < point_count; point++) {
            hash = guard_hash_mix(hash, (uint32_t)path->points[point].x);
            hash = guard_hash_mix(hash, (uint32_t)path->points[point].y);
        }
    }
    return hash;
}

static int check_mismatched_commit_guard(FILE *file, const WorldGenContext *context) {
    WorldGenContext mismatch = *context;
    uint64_t world_before = committed_world_hash(context);
    uint64_t paths_before = committed_paths_hash();
    int physical_valid_before = world_physical_state_valid();
    int physical_revision_before = world_physical_state_revision();
    int rejected;
    int ok;
    mismatch.hydrology_token ^= UINT64_C(0x9e3779b97f4a7c15);
    if (mismatch.hydrology_token == 0) mismatch.hydrology_token = 1;
    rejected = !world_gen_commit_prepared(&mismatch);
    ok = river_network_view_matches_context(context) &&
         !river_network_view_matches_context(&mismatch) && rejected &&
         world_before == committed_world_hash(context) &&
         paths_before == committed_paths_hash() &&
         physical_valid_before == world_physical_state_valid() &&
         physical_revision_before == world_physical_state_revision();
    coverage.commit_guard_checked = 1;
    coverage.commit_guard_ok = ok;
    fprintf(file, "case=hydrology_commit_guard token=%llu mismatch=%llu rejected=%d "
                  "physical_revision=%d ok=%d\n",
            (unsigned long long)context->hydrology_token,
            (unsigned long long)mismatch.hydrology_token, rejected,
            physical_revision_before, ok);
    return ok;
}

void game_worldgen_hydrology_probe_reset(void) {
    memset(&coverage, 0, sizeof(coverage));
    game_worldgen_hydrology_direction_probe_reset();
}

static int validate_topology(const WorldGenContext *context, const RiverNetworkView *view,
                             int *receiver_errors, int *order_errors,
                             int *width_errors, int *flow_errors) {
    int *position = (int *)malloc((size_t)view->tile_count * sizeof(*position));
    int i;
    if (!position) return 0;
    for (i = 0; i < view->tile_count; i++) position[i] = -1;
    for (i = 0; i < view->diagnostics.topological_cells; i++) {
        int index = view->topological_order[i];
        if (index < 0 || index >= view->tile_count || position[index] >= 0) {
            (*receiver_errors)++;
            continue;
        }
        position[index] = i;
    }
    for (i = 0; i < view->tile_count; i++) {
        int receiver;
        if (!context->land_mask[i]) continue;
        if (position[i] < 0) (*receiver_errors)++;
        receiver = view->receiver[i];
        if (receiver < 0) {
            if (!(view->cell_flags[i] & (RIVER_CELL_CLOSED_BASIN | RIVER_CELL_EDGE_OUTLET))) {
                (*receiver_errors)++;
            }
            continue;
        }
        if (receiver >= view->tile_count || receiver == i) {
            (*receiver_errors)++;
            continue;
        }
        if (context->land_mask[receiver]) {
            if (position[receiver] < 0 || position[receiver] >= position[i]) (*receiver_errors)++;
            if (view->flow[receiver] < view->flow[i]) (*flow_errors)++;
            if ((view->cell_flags[i] & RIVER_CELL_CHANNEL) &&
                (view->cell_flags[receiver] & RIVER_CELL_CHANNEL)) {
                if (view->order[receiver] < view->order[i]) (*order_errors)++;
                if (view->width_field[receiver] < view->width_field[i]) (*width_errors)++;
            }
        }
    }
    free(position);
    return 1;
}

static int validate_segments(const WorldGenContext *context, const RiverNetworkView *view,
                             int *duplicate_edges, int *distribution_errors) {
    uint8_t *ordinary_from = (uint8_t *)calloc((size_t)view->tile_count, 1);
    int i;
    if (!ordinary_from) return 0;
    for (i = 0; i < view->segment_count; i++) {
        const RiverNetworkSegment *segment = &view->segments[i];
        if (segment->from < 0 || segment->from >= view->tile_count ||
            segment->to < 0 || segment->to >= view->tile_count) {
            (*duplicate_edges)++;
            continue;
        }
        if (segment->kind == RIVER_SEGMENT_ORDINARY) {
            if (ordinary_from[segment->from]) (*duplicate_edges)++;
            ordinary_from[segment->from] = 1;
            if (segment->to != view->receiver[segment->from]) (*duplicate_edges)++;
        }
    }
    for (i = 0; i < view->distributary_count; i++) {
        const RiverDistributary *first = &view->distributaries[i];
        uint64_t total = 0;
        int count = 0;
        int j;
        int seen_before = 0;
        for (j = 0; j < i; j++) {
            if (view->distributaries[j].from == first->from) seen_before = 1;
        }
        if (seen_before) continue;
        for (j = i; j < view->distributary_count; j++) {
            const RiverDistributary *branch = &view->distributaries[j];
            if (branch->from != first->from) continue;
            total += branch->flow;
            count++;
            if (branch->to < 0 || branch->to >= view->tile_count ||
                context->land_mask[branch->to]) (*distribution_errors)++;
        }
        if (first->from < 0 || first->from >= view->tile_count ||
            !(view->cell_flags[first->from] & RIVER_CELL_DELTA) ||
            count != first->branch_count || count < 2 || count > 3 ||
            total != view->flow[first->from]) (*distribution_errors)++;
    }
    free(ordinary_from);
    return 1;
}

static int validate_traced_paths(const WorldGenContext *context, const RiverNetworkView *view,
                                 int *errors) {
    const RiverPath *paths = (const RiverPath *)context->staged_river_paths;
    uint8_t *ordinary_from = (uint8_t *)calloc((size_t)view->tile_count, 1);
    int ordinary = 0;
    int distributaries = 0;
    int i;
    if (!ordinary_from || context->staged_river_path_count !=
        context->staged_river_paths_required) {
        free(ordinary_from);
        return 0;
    }
    for (i = 0; i < context->staged_river_path_count; i++) {
        const RiverPath *path = &paths[i];
        int point;
        if (path->point_count < 2 || path->point_count > MAX_RIVER_POINTS) {
            (*errors)++;
            continue;
        }
        for (point = 0; point + 1 < path->point_count; point++) {
            int from = path->points[point].y * view->width + path->points[point].x;
            int to = path->points[point + 1].y * view->width + path->points[point + 1].x;
            if (from < 0 || from >= view->tile_count || to < 0 || to >= view->tile_count) {
                (*errors)++;
                continue;
            }
            if (point == 0 && path->point_count == 2 &&
                (view->cell_flags[from] & RIVER_CELL_DELTA)) {
                distributaries++;
                if (context->land_mask[to]) (*errors)++;
                continue;
            }
            if (view->receiver[from] != to || ordinary_from[from]) (*errors)++;
            ordinary_from[from] = 1;
            ordinary++;
            if (point + 2 < path->point_count &&
                !(view->cell_flags[to] & RIVER_CELL_CHANNEL)) (*errors)++;
        }
    }
    if (ordinary != view->diagnostics.ordinary_segments ||
        distributaries != view->distributary_count) (*errors)++;
    free(ordinary_from);
    return 1;
}

static int identity_count_errors(const RiverNetworkView *view) {
    int channels = 0;
    int sources = 0;
    int confluences = 0;
    int lakes = 0;
    int closed = 0;
    int mouths = 0;
    int deltas = 0;
    int i;
    for (i = 0; i < view->tile_count; i++) {
        uint16_t flags = view->cell_flags[i];
        channels += (flags & RIVER_CELL_CHANNEL) != 0;
        sources += (flags & RIVER_CELL_SOURCE) != 0;
        confluences += (flags & RIVER_CELL_CONFLUENCE) != 0;
        lakes += (flags & RIVER_CELL_LAKE) != 0;
        closed += (flags & RIVER_CELL_CLOSED_BASIN) != 0;
        mouths += (flags & RIVER_CELL_MOUTH) != 0;
        deltas += (flags & RIVER_CELL_DELTA) != 0;
    }
    return (channels != view->diagnostics.channel_cells) +
           (sources != view->diagnostics.sources) +
           (confluences != view->diagnostics.confluences) +
           (lakes != view->diagnostics.lake_cells) +
           (closed != view->diagnostics.closed_basins) +
           (mouths != view->diagnostics.mouths) +
           (deltas != view->diagnostics.deltas);
}

static int build_macro_twin(const WorldGenContext *context, WorldGenContext *baseline) {
    if (!world_gen_context_create(baseline, &context->config, context->width,
                                  context->height, context->master_seed)) return 0;
    if (!world_gen_build_elevation_and_mask(baseline) ||
        !world_gen_apply_mountains(baseline)) return 0;
    world_gen_finalize_elevation(baseline);
    return world_gen_build_climate_fields(baseline) &&
           world_gen_classify_macro_climate(baseline);
}

static void seed_hydro_distance(const WorldGenContext *context, const RiverNetworkView *view,
                                int *distance, int *queue, int *tail) {
    int i;
    for (i = 0; i < context->tile_count; i++) {
        distance[i] = -1;
        if (view->cell_flags[i] & (RIVER_CELL_CHANNEL | RIVER_CELL_LAKE)) {
            distance[i] = 0;
            queue[(*tail)++] = i;
        }
    }
}

static void build_hydro_distance(const WorldGenContext *context, const RiverNetworkView *view,
                                 int *distance, int *queue) {
    static const int dx[4] = {1, -1, 0, 0};
    static const int dy[4] = {0, 0, 1, -1};
    int head = 0;
    int tail = 0;
    seed_hydro_distance(context, view, distance, queue, &tail);
    while (head < tail) {
        int index = queue[head++];
        int x = index % context->width;
        int y = index / context->width;
        int direction;
        if (distance[index] >= 6) continue;
        for (direction = 0; direction < 4; direction++) {
            int nx = x + dx[direction];
            int ny = y + dy[direction];
            int next;
            if (!world_gen_context_in_bounds(context, nx, ny)) continue;
            next = world_gen_context_index(context, nx, ny);
            if (!context->land_mask[next] || distance[next] >= 0) continue;
            distance[next] = distance[index] + 1;
            queue[tail++] = next;
        }
    }
}

static int classifier_base_fertility(const WorldGenContext *context, int index) {
    int value = context->moisture[index] * 3 / 5 + context->temperature[index] / 5 -
                context->slope[index] * 2;
    if (context->geography[index] == GEO_PLAIN || context->geography[index] == GEO_BASIN) value += 10;
    if (context->geography[index] == GEO_DELTA) value += 28;
    if (context->geography[index] == GEO_MOUNTAIN || context->geography[index] == GEO_VOLCANO) value -= 22;
    return clamp_int(value, 0, 100);
}

static int check_hydro_decay(FILE *file, const char *label,
                             const WorldGenContext *context, const RiverNetworkView *view) {
    WorldGenContext baseline;
    int *distance = NULL;
    int *queue = NULL;
    uint64_t near_moisture = 0;
    uint64_t far_moisture = 0;
    uint64_t near_fertility = 0;
    uint64_t far_fertility = 0;
    int near_count = 0;
    int far_count = 0;
    int negative_moisture = 0;
    int climate_errors = 0;
    int ok = 0;
    int i;
    memset(&baseline, 0, sizeof(baseline));
    if (!build_macro_twin(context, &baseline)) goto done;
    distance = (int *)malloc((size_t)context->tile_count * sizeof(*distance));
    queue = (int *)malloc((size_t)context->tile_count * sizeof(*queue));
    if (!distance || !queue) goto done;
    build_hydro_distance(context, view, distance, queue);
    for (i = 0; i < context->tile_count; i++) {
        int moisture_delta;
        int fertility_bonus;
        if (!context->land_mask[i]) continue;
        moisture_delta = context->moisture[i] - baseline.moisture[i];
        fertility_bonus = context->soil_fertility[i] - classifier_base_fertility(context, i);
        if (moisture_delta < 0) negative_moisture++;
        if (fertility_bonus < 0) fertility_bonus = 0;
        if (context->climate[i] != baseline.climate[i] &&
            !(context->river_flags[i] & (WORLD_GEN_RIVER_LAKE | WORLD_GEN_RIVER_DELTA))) {
            climate_errors++;
        }
        if (distance[i] >= 0 && distance[i] <= 1) {
            near_moisture += moisture_delta > 0 ? (uint64_t)moisture_delta : 0;
            near_fertility += (uint64_t)fertility_bonus;
            near_count++;
        } else if (distance[i] >= 4 && distance[i] <= 6) {
            far_moisture += moisture_delta > 0 ? (uint64_t)moisture_delta : 0;
            far_fertility += (uint64_t)fertility_bonus;
            far_count++;
        }
    }
    ok = near_count > 0 && far_count > 0 && negative_moisture == 0 && climate_errors == 0 &&
         near_moisture * (uint64_t)far_count > far_moisture * (uint64_t)near_count &&
         near_fertility * (uint64_t)far_count > far_fertility * (uint64_t)near_count;
done:
    fprintf(file, "case=hydrology_decay label=%s near_count=%d far_count=%d "
                  "near_moisture=%llu far_moisture=%llu near_fertility=%llu far_fertility=%llu "
                  "negative=%d climate_errors=%d ok=%d\n",
            label, near_count, far_count,
            (unsigned long long)near_moisture, (unsigned long long)far_moisture,
            (unsigned long long)near_fertility, (unsigned long long)far_fertility,
            negative_moisture, climate_errors, ok);
    free(distance);
    free(queue);
    world_gen_context_destroy(&baseline);
    return ok;
}

int game_worldgen_hydrology_probe_check_context(FILE *file, const char *label,
                                                const WorldGenContext *context,
                                                int check_decay) {
    const RiverNetworkView *view = river_network_latest_view();
    RiverGenerationDiagnostics diagnostics;
    int receiver_errors = 0;
    int manual_order_errors = 0;
    int manual_width_errors = 0;
    int manual_flow_errors = 0;
    int duplicate_edges = 0;
    int distribution_errors = 0;
    int identity_errors;
    int required_paths;
    int diagnostics_ok;
    int manual_ok;
    int legacy_ok;
    int ok;
    if (!file || !label || !context || !view) return 0;
    diagnostics = view->diagnostics;
    identity_errors = identity_count_errors(view);
    manual_ok = validate_topology(context, view, &receiver_errors, &manual_order_errors,
                                  &manual_width_errors, &manual_flow_errors) &&
                validate_segments(context, view, &duplicate_edges, &distribution_errors) &&
                validate_traced_paths(context, view, &duplicate_edges) &&
                game_worldgen_hydrology_invariant_probe_check(file, label, context, view) &&
                game_worldgen_lake_semantics_probe_check(file, label, context) &&
                game_worldgen_hydrology_direction_probe_check(file, label, context, view);
    diagnostics_ok = diagnostics.invalid_receivers == 0 && diagnostics.inland_dead_ends == 0 &&
                     diagnostics.cycle_errors == 0 && diagnostics.flow_conservation_errors == 0 &&
                     diagnostics.width_regressions == 0 && diagnostics.order_errors == 0 &&
                     diagnostics.duplicate_edges == 0 && diagnostics.crossing_errors == 0 &&
                     diagnostics.channel_cells > 0 && diagnostics.sources > 0 &&
                     diagnostics.topological_cells == diagnostics.land_cells;
    manual_ok &= receiver_errors == 0 && manual_order_errors == 0 && manual_width_errors == 0 &&
                 manual_flow_errors == 0 && duplicate_edges == 0 && distribution_errors == 0 &&
                 identity_errors == 0;
    if (!coverage.commit_guard_checked) manual_ok &= check_mismatched_commit_guard(file, context);
    required_paths = river_network_count_legacy_paths(context);
    legacy_ok = required_paths > 0 && required_paths == context->staged_river_paths_required &&
        required_paths == context->staged_river_path_count && river_paths_validate(
            (const RiverPath *)context->staged_river_paths, context->staged_river_path_count,
            context->width, context->height);
    if (check_decay) { coverage.decay_checked = 1; manual_ok &= check_hydro_decay(file, label, context, view); }
    coverage.cases++;
    coverage.sources += diagnostics.sources;
    coverage.confluences += diagnostics.confluences;
    coverage.lakes += diagnostics.lake_cells;
    coverage.closed_basins += diagnostics.closed_basins;
    coverage.salt_lakes += diagnostics.salt_lakes;
    coverage.mouths += diagnostics.mouths;
    coverage.deltas += diagnostics.deltas;
    coverage.distributaries += diagnostics.distributaries;
    ok = diagnostics_ok && manual_ok && legacy_ok;
    fprintf(file, "case=hydrology_graph label=%s land=%d topo=%d channels=%d sources=%d "
                  "confluences=%d lakes=%d closed=%d salt=%d mouths=%d deltas=%d branches=%d "
                  "receiver_errors=%d flow_errors=%d order_errors=%d width_errors=%d "
                  "duplicates=%d crossings=%d distribution_errors=%d identity_errors=%d ok=%d\n",
            label, diagnostics.land_cells, diagnostics.topological_cells,
            diagnostics.channel_cells, diagnostics.sources, diagnostics.confluences,
            diagnostics.lake_cells, diagnostics.closed_basins, diagnostics.salt_lakes, diagnostics.mouths,
            diagnostics.deltas, diagnostics.distributaries, receiver_errors,
            diagnostics.flow_conservation_errors + manual_flow_errors,
            diagnostics.order_errors + manual_order_errors,
            diagnostics.width_regressions + manual_width_errors,
            diagnostics.duplicate_edges + duplicate_edges, diagnostics.crossing_errors,
            distribution_errors, identity_errors, diagnostics_ok && manual_ok);
    fprintf(file, "case=hydrology_paths label=%s required=%d copied=%d truncated=%d "
                  "valid=%d ok=%d\n", label, required_paths, context->staged_river_path_count,
            required_paths - context->staged_river_path_count, river_path_count_valid(
                context->staged_river_path_count, context->width, context->height), legacy_ok);
    return ok;
}

int game_worldgen_hydrology_probe_finish(FILE *file) {
    int ok = game_worldgen_hydrology_direction_probe_finish(file) &&
             coverage.cases > 0 && coverage.sources > 0 && coverage.confluences > 0 &&
             coverage.lakes > 0 && coverage.closed_basins > 0 && coverage.mouths > 0 &&
             coverage.salt_lakes > 0 && coverage.deltas > 0 &&
             coverage.distributaries >= 2 && coverage.decay_checked &&
             coverage.commit_guard_checked && coverage.commit_guard_ok;
    if (!file) return 0;
    fprintf(file, "case=hydrology_coverage cases=%d sources=%d confluences=%d lakes=%d closed=%d salt=%d "
                  "mouths=%d deltas=%d distributaries=%d decay=%d commit_guard=%d ok=%d\n",
            coverage.cases, coverage.sources, coverage.confluences, coverage.lakes,
            coverage.closed_basins, coverage.salt_lakes, coverage.mouths, coverage.deltas,
            coverage.distributaries, coverage.decay_checked, coverage.commit_guard_ok, ok);
    return ok;
}
