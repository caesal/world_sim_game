#include "world/rivers.h"

#include "core/game_types.h"
#include "world/river_drainage.h"
#include "world/river_flow.h"
#include "world/river_hydroclimate.h"
#include "world/river_mouths.h"
#include "world/river_path_validation.h"
#include "world/river_presentation_state.h"
#include "world/river_state.h"
#include "world/world_physical_state.h"

#include <stdlib.h>
#include <string.h>

static RiverGenerationDiagnostics persisted_diagnostics;
static RiverGenerationDiagnostics committed_diagnostics;
static int committed_diagnostics_valid;
static int committed_physical_revision;
static uint32_t committed_presentation_revision;

static uint64_t token_mix(uint64_t hash, uint64_t value) {
    int byte_index;
    for (byte_index = 0; byte_index < 8; byte_index++) {
        hash ^= (uint8_t)(value >> (byte_index * 8));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t hydrology_token_for_context(const WorldGenContext *context) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int i;
    if (!context) return 0;
    hash = token_mix(hash, (uint32_t)context->width);
    hash = token_mix(hash, (uint32_t)context->height);
    hash = token_mix(hash, context->master_seed);
    hash = token_mix(hash, (uint32_t)context->sea_level);
    hash = token_mix(hash, (uint32_t)context->config.ocean);
    hash = token_mix(hash, (uint32_t)context->config.continent);
    hash = token_mix(hash, (uint32_t)context->config.relief);
    hash = token_mix(hash, (uint32_t)context->config.moisture);
    hash = token_mix(hash, (uint32_t)context->config.drought);
    hash = token_mix(hash, (uint32_t)context->config.vegetation);
    hash = token_mix(hash, (uint32_t)context->config.bias_forest);
    hash = token_mix(hash, (uint32_t)context->config.bias_desert);
    hash = token_mix(hash, (uint32_t)context->config.bias_mountain);
    hash = token_mix(hash, (uint32_t)context->config.bias_wetland);
    for (i = 0; i < context->tile_count; i++) {
        uint64_t terrain = context->land_mask[i] |
                           ((uint64_t)(uint16_t)context->elevation[i] << 8) |
                           ((uint64_t)(uint16_t)context->relative_altitude[i] << 24) |
                           ((uint64_t)(uint16_t)context->slope[i] << 40);
        uint64_t climate = (uint16_t)context->moisture[i] |
                           ((uint64_t)(uint16_t)context->temperature[i] << 16) |
                           ((uint64_t)(uint16_t)context->precipitation[i] << 32) |
                           ((uint64_t)context->climate[i] << 48) |
                           ((uint64_t)context->wind_direction16[i] << 56);
        uint64_t landform = (uint16_t)context->base_elevation[i] |
                            ((uint64_t)(uint16_t)context->curvature[i] << 16) |
                            ((uint64_t)(uint16_t)context->mountain_uplift[i] << 32) |
                            ((uint64_t)(uint16_t)context->ocean_distance[i] << 48);
        hash = token_mix(hash, terrain);
        hash = token_mix(hash, climate);
        hash = token_mix(hash, landform);
        hash = token_mix(hash, context->wind_speed[i]);
    }
    return hash != 0 ? hash : UINT64_C(0xcbf29ce484222325);
}

static int geography_is_land(Geography geography) {
    return geography != GEO_OCEAN && geography != GEO_LAKE && geography != GEO_BAY;
}

static uint16_t context_flags(uint16_t flags) {
    uint16_t mapped = 0;
    if (flags & RIVER_CELL_CHANNEL) mapped |= WORLD_GEN_RIVER_CHANNEL;
    if (flags & RIVER_CELL_LAKE) mapped |= WORLD_GEN_RIVER_LAKE;
    if (flags & RIVER_CELL_MOUTH) mapped |= WORLD_GEN_RIVER_MOUTH;
    if (flags & RIVER_CELL_DELTA) mapped |= WORLD_GEN_RIVER_DELTA;
    if (flags & RIVER_CELL_CONFLUENCE) mapped |= WORLD_GEN_RIVER_CONFLUENCE;
    if (flags & RIVER_CELL_CLOSED_BASIN) mapped |= WORLD_GEN_RIVER_CLOSED_BASIN;
    if (flags & RIVER_CELL_SOURCE) mapped |= WORLD_GEN_RIVER_SOURCE;
    if (flags & RIVER_CELL_DISTRIBUTARY) mapped |= WORLD_GEN_RIVER_DISTRIBUTARY;
    if (flags & RIVER_CELL_SALT_LAKE) mapped |= WORLD_GEN_RIVER_SALT_LAKE;
    return mapped;
}

static int validate_unique_ordinary_edges(RiverGenerationState *state) {
    int i;

    memset(state->visited, 0, (size_t)state->input.tile_count * sizeof(*state->visited));
    for (i = 0; i < state->segment_count; i++) {
        const RiverNetworkSegment *segment = &state->segments[i];
        if (segment->kind != RIVER_SEGMENT_ORDINARY) continue;
        if (segment->from < 0 || segment->from >= state->input.tile_count) {
            state->diagnostics.invalid_receivers++;
            continue;
        }
        if (state->visited[segment->from]) state->diagnostics.duplicate_edges++;
        state->visited[segment->from] = 1;
    }
    memset(state->visited, 0, (size_t)state->input.tile_count * sizeof(*state->visited));
    for (i = 0; i < state->segment_count; i++) {
        const RiverNetworkSegment *segment = &state->segments[i];
        int from_x = segment->from % state->input.width;
        int from_y = segment->from / state->input.width;
        int to_x = segment->to % state->input.width;
        int to_y = segment->to / state->input.width;
        int square;
        uint8_t orientation;
        if (segment->to < 0 || segment->to >= state->input.tile_count) continue;
        if (abs(from_x - to_x) != 1 || abs(from_y - to_y) != 1) continue;
        square = (from_y < to_y ? from_y : to_y) * state->input.width +
                 (from_x < to_x ? from_x : to_x);
        orientation = (from_x - to_x) * (from_y - to_y) > 0 ? 1u : 2u;
        if (state->visited[square] & (orientation == 1u ? 2u : 1u)) {
            state->diagnostics.crossing_errors++;
        }
        state->visited[square] |= orientation;
    }
    return state->diagnostics.duplicate_edges == 0 &&
           state->diagnostics.invalid_receivers == 0 && state->diagnostics.crossing_errors == 0;
}

int world_gen_hydrology_build(WorldGenContext *context) {
    RiverGenerationInput input;
    RiverGenerationState *state;
    int i;
    int ok;

    memset(&persisted_diagnostics, 0, sizeof(persisted_diagnostics));
    if (!context || context->tile_count <= 0) return 0;
    context->hydrology_token = hydrology_token_for_context(context);
    memset(&input, 0, sizeof(input));
    input.width = context->width;
    input.height = context->height;
    input.tile_count = context->tile_count;
    input.land_mask = context->land_mask;
    input.elevation = context->elevation;
    input.relative_altitude = context->relative_altitude;
    input.slope = context->slope;
    input.moisture = context->moisture;
    input.temperature = context->temperature;
    input.precipitation = context->precipitation;
    input.seed = context->phase_seed[WORLD_GEN_PHASE_HYDROLOGY];
    input.generation_token = context->hydrology_token;
    input.moisture_bias = context->config.moisture;
    input.wetland_bias = context->config.bias_wetland;
    input.receiver = context->drainage_receiver;
    input.basin = context->drainage_basin;
    input.topological_order = context->topological_order;
    input.runoff = context->runoff;
    input.flow = context->river_flow;
    input.upstream_count = context->upstream_count;
    input.order = context->river_order;
    input.width_field = context->river_width;
    input.soil_fertility = context->soil_fertility;
    state = river_state_prepare(&input);
    if (!state) {
        RiverGenerationDiagnostics allocation_diagnostics = {0};
        river_state_last_diagnostics(&allocation_diagnostics);
        if (allocation_diagnostics.workspace_allocation_errors > 0) {
            persisted_diagnostics = allocation_diagnostics;
        }
        return 0;
    }
    ok = river_drainage_build(state);
    if (ok) ok = river_flow_build(state);
    if (ok) ok = river_mouths_build(state);
    if (ok) ok = river_hydroclimate_apply(state);
    if (ok) ok = river_flow_build_segments(state);
    if (ok) ok = validate_unique_ordinary_edges(state);
    for (i = 0; i < context->tile_count; i++) {
        context->river_flags[i] = context_flags(state->cell_flags[i]);
    }
    context->topological_count = state->topological_count;
    river_state_publish_view(state);
    persisted_diagnostics = state->diagnostics;
    return ok;
}

const RiverNetworkView *river_network_latest_view(void) {
    return river_state_latest_view();
}

int river_network_view_matches_context(const WorldGenContext *context) {
    const RiverNetworkView *view = river_state_latest_view();
    return context && view && context->hydrology_token != 0 &&
           view->width == context->width && view->height == context->height &&
           view->tile_count == context->tile_count &&
           river_state_view_matches(context->hydrology_token);
}

void river_generation_last_diagnostics(RiverGenerationDiagnostics *out) {
    if (out) *out = persisted_diagnostics;
}

int river_generation_committed_diagnostics(RiverGenerationDiagnostics *out) {
    if (!out || !committed_diagnostics_valid || !world_physical_state_valid() ||
        committed_physical_revision != world_physical_state_revision() ||
        committed_presentation_revision != river_presentation_state_revision() ||
        committed_diagnostics.legacy_paths_required != river_path_count ||
        committed_diagnostics.legacy_paths_truncated != 0) return 0;
    *out = committed_diagnostics;
    return 1;
}

void river_generation_note_commit(const RiverGenerationDiagnostics *diagnostics) {
    if (!diagnostics) return;
    committed_diagnostics = *diagnostics;
    committed_physical_revision = world_physical_state_revision();
    committed_presentation_revision = river_presentation_state_revision();
    committed_diagnostics_valid = 1;
}

static void clear_path(RiverPath *path) {
    memset(path, 0, sizeof(*path));
    path->active = 1;
}

static void add_point(RiverPath *path, int index, int width) {
    if (path->point_count >= MAX_RIVER_POINTS || index < 0) return;
    path->points[path->point_count].x = index % width;
    path->points[path->point_count].y = index / width;
    path->point_count++;
}

static int append_path(RiverPath *paths, int capacity, int required,
                       const RiverPath *path) {
    if (path->point_count < 2) return required;
    if (required < capacity && paths) paths[required] = *path;
    return required + 1;
}

static int trace_ordinary_paths(const RiverNetworkView *view, uint8_t *starts,
                                RiverPath *paths, int capacity) {
    int required = 0;
    int i;

    for (i = 0; i < view->tile_count; i++) {
        uint16_t flags = view->cell_flags[i];
        int receiver = view->receiver[i];
        if (!(flags & RIVER_CELL_CHANNEL)) continue;
        if (!(flags & RIVER_CELL_LAKE) && (flags & (RIVER_CELL_SOURCE | RIVER_CELL_CONFLUENCE))) {
            starts[i] = 1;
        }
        if ((flags & RIVER_CELL_LAKE) && receiver >= 0 && receiver < view->tile_count &&
            !(view->cell_flags[receiver] & RIVER_CELL_LAKE)) starts[i] = 1;
    }
    for (i = 0; i < view->segment_count; i++) {
        const RiverNetworkSegment *segment = &view->segments[i];
        int to = segment->to;
        if (segment->kind != RIVER_SEGMENT_ORDINARY || to < 0 || to >= view->tile_count) continue;
        if (!(view->cell_flags[to] & RIVER_CELL_CHANNEL)) continue;
        if (view->cell_flags[to] & RIVER_CELL_LAKE) continue;
        if (view->width_field[to] != view->width_field[segment->from] ||
            (view->cell_flags[to] & (RIVER_CELL_CONFLUENCE | RIVER_CELL_MOUTH |
             RIVER_CELL_DELTA | RIVER_CELL_CLOSED_BASIN))) starts[to] = 1;
    }
    for (i = 0; i < view->tile_count; i++) {
        int current;
        if (!starts[i] || !(view->cell_flags[i] & RIVER_CELL_CHANNEL)) continue;
        if (view->cell_flags[i] & RIVER_CELL_DELTA) continue;
        current = i;
        while (current >= 0) {
            RiverPath path;
            int chunk_continues = 0;
            int first = current;
            clear_path(&path);
            path.width = view->width_field[current];
            path.flow = view->flow[current];
            path.order = view->order[current];
            add_point(&path, current, view->width);
            while (path.point_count < MAX_RIVER_POINTS) {
                int receiver = view->receiver[current];
                if (receiver < 0 || receiver >= view->tile_count) break;
                add_point(&path, receiver, view->width);
                if (view->flow[receiver] > (uint32_t)path.flow) path.flow = view->flow[receiver];
                if (!(view->cell_flags[current] & RIVER_CELL_LAKE) &&
                    (view->cell_flags[receiver] & RIVER_CELL_LAKE)) break;
                if (!(view->cell_flags[receiver] & RIVER_CELL_CHANNEL)) break;
                if (!view->cell_flags[receiver] || !(view->cell_flags[receiver] & RIVER_CELL_LAND)) break;
                if (starts[receiver] && receiver != first) break;
                current = receiver;
                if (path.point_count == MAX_RIVER_POINTS && view->receiver[current] >= 0) {
                    chunk_continues = 1;
                }
            }
            required = append_path(paths, capacity, required, &path);
            if (!chunk_continues) break;
        }
    }
    return required;
}

static int append_distributary_paths(const RiverNetworkView *view, RiverPath *paths,
                                     int capacity, int required) {
    int i;

    for (i = 0; i < view->distributary_count; i++) {
        const RiverDistributary *branch = &view->distributaries[i];
        RiverPath path;
        clear_path(&path);
        path.width = branch->width;
        path.flow = branch->flow;
        path.order = branch->order;
        add_point(&path, branch->from, view->width);
        add_point(&path, branch->to, view->width);
        required = append_path(paths, capacity, required, &path);
    }
    return required;
}

static int build_legacy_paths(RiverGenerationState *state, const RiverNetworkView *view,
                              RiverPath *paths, int capacity) {
    uint8_t *starts = state->visited;
    int required;

    memset(starts, 0, (size_t)view->tile_count * sizeof(*starts));
    if (paths && capacity > 0) memset(paths, 0, (size_t)capacity * sizeof(*paths));
    required = trace_ordinary_paths(view, starts, paths, capacity);
    return append_distributary_paths(view, paths, capacity, required);
}

int river_network_count_legacy_paths(const WorldGenContext *context) {
    RiverGenerationState *state = river_state_current();
    const RiverNetworkView *view = river_state_latest_view();

    if (!view || !state || !river_network_view_matches_context(context)) return -1;
    return build_legacy_paths(state, view, NULL, 0);
}

int river_network_copy_legacy_paths(RiverPath *paths, int capacity) {
    RiverGenerationState *state = river_state_current();
    const RiverNetworkView *view = river_state_latest_view();
    int required;

    if (!view || !state || capacity < 0 || (capacity > 0 && !paths)) return 0;
    required = build_legacy_paths(state, view, paths, capacity);
    state->diagnostics.legacy_paths_required = required;
    state->diagnostics.legacy_paths_truncated = required > capacity ? required - capacity : 0;
    river_state_refresh_view_diagnostics(state);
    persisted_diagnostics = state->diagnostics;
    return required <= capacity ? required : -1;
}

int river_network_legacy_path_count_required(void) {
    return persisted_diagnostics.legacy_paths_required;
}

void river_network_release_transient(void) {
    RiverGenerationDiagnostics diagnostics = persisted_diagnostics;
    river_state_release();
    persisted_diagnostics = diagnostics;
}

static void fill_compatibility_context(WorldGenContext *context) {
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int index = y * MAP_W + x;
            int min_elevation = world[y][x].elevation;
            int max_elevation = min_elevation;
            int dx;
            int dy;
            context->land_mask[index] = (uint8_t)geography_is_land(world[y][x].geography);
            context->elevation[index] = (int16_t)world[y][x].elevation;
            context->relative_altitude[index] = (int16_t)world[y][x].elevation;
            context->moisture[index] = (int16_t)world[y][x].moisture;
            context->temperature[index] = (int16_t)world[y][x].temperature;
            context->precipitation[index] = (int16_t)world[y][x].moisture;
            for (dy = -1; dy <= 1; dy++) {
                for (dx = -1; dx <= 1; dx++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
                    if (world[ny][nx].elevation < min_elevation) min_elevation = world[ny][nx].elevation;
                    if (world[ny][nx].elevation > max_elevation) max_elevation = world[ny][nx].elevation;
                }
            }
            context->slope[index] = (int16_t)(max_elevation - min_elevation);
        }
    }
}

void generate_rivers(int moisture, int bias_wetland) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    WorldGenContext context;
    int x;
    int y;
    RiverPath *paths = NULL;
    int required = 0;

    config.moisture = moisture;
    config.bias_wetland = bias_wetland;
    if (!world_gen_context_create(&context, &config, MAP_W, MAP_H, 0x52495652u)) return;
    fill_compatibility_context(&context);
    if (world_gen_hydrology_build(&context)) {
        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                int index = y * MAP_W + x;
                world[y][x].river = (context.river_flags[index] & WORLD_GEN_RIVER_CHANNEL) != 0;
            }
        }
        required = river_network_count_legacy_paths(&context);
        if (river_path_count_valid(required, MAP_W, MAP_H) && required > 0)
            paths = (RiverPath *)calloc((size_t)required, sizeof(*paths));
        if ((required == 0 || paths) &&
            river_network_copy_legacy_paths(paths, required) == required &&
            river_paths_validate(paths, required, MAP_W, MAP_H)) {
            river_presentation_state_adopt(&paths, required, MAP_W, MAP_H);
        }
    }
    free(paths);
    world_gen_context_destroy(&context);
}
