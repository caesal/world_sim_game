#include "game/game_worldgen_lake_qualification_probe.h"

#include "world/river_lake_qualification.h"

#include <stdint.h>
#include <string.h>

enum {
    QUALIFIER_W = 17,
    QUALIFIER_H = 17,
    QUALIFIER_COUNT = QUALIFIER_W * QUALIFIER_H
};

typedef struct {
    RiverGenerationState state;
    uint8_t land[QUALIFIER_COUNT];
    int16_t elevation[QUALIFIER_COUNT];
    int16_t precipitation[QUALIFIER_COUNT];
    int32_t receiver[QUALIFIER_COUNT];
    int32_t topological_order[QUALIFIER_COUNT];
    int32_t filled_elevation[QUALIFIER_COUNT];
    int32_t heap[QUALIFIER_COUNT];
    int32_t main_stem[QUALIFIER_COUNT];
    int32_t dominant_parent[QUALIFIER_COUNT];
    uint32_t check_flow[QUALIFIER_COUNT];
    uint32_t upstream_count[QUALIFIER_COUNT];
    uint8_t visited[QUALIFIER_COUNT];
    uint8_t max_upstream_order[QUALIFIER_COUNT];
    uint8_t max_upstream_count[QUALIFIER_COUNT];
} QualificationFixture;

static const int CARDINAL_DX[4] = {1, 0, -1, 0};
static const int CARDINAL_DY[4] = {0, 1, 0, -1};

static int qualifier_index(int x, int y) {
    return y * QUALIFIER_W + x;
}

static void qualifier_fixture_init(QualificationFixture *fixture) {
    int i;
    memset(fixture, 0, sizeof(*fixture));
    fixture->state.input.width = QUALIFIER_W;
    fixture->state.input.height = QUALIFIER_H;
    fixture->state.input.tile_count = QUALIFIER_COUNT;
    fixture->state.input.land_mask = fixture->land;
    fixture->state.input.elevation = fixture->elevation;
    fixture->state.input.precipitation = fixture->precipitation;
    fixture->state.input.receiver = fixture->receiver;
    fixture->state.input.topological_order = fixture->topological_order;
    fixture->state.input.seed = UINT32_C(0x71a5c0de);
    fixture->state.filled_elevation = fixture->filled_elevation;
    fixture->state.heap = fixture->heap;
    fixture->state.main_stem = fixture->main_stem;
    fixture->state.dominant_parent = fixture->dominant_parent;
    fixture->state.check_flow = fixture->check_flow;
    fixture->state.published_upstream_count = fixture->upstream_count;
    fixture->state.visited = fixture->visited;
    fixture->state.max_upstream_order = fixture->max_upstream_order;
    fixture->state.max_upstream_count = fixture->max_upstream_count;
    fixture->state.topological_count = QUALIFIER_COUNT;
    for (i = 0; i < QUALIFIER_COUNT; i++) {
        fixture->land[i] = 1;
        fixture->elevation[i] = 30;
        fixture->precipitation[i] = 75;
        fixture->receiver[i] = -1;
        fixture->topological_order[i] = i;
        fixture->filled_elevation[i] = 30;
        fixture->main_stem[i] = -1;
        fixture->dominant_parent[i] = -1;
        fixture->check_flow[i] = 4;
        fixture->upstream_count[i] = 1;
    }
}

static void qualifier_mark_basin(QualificationFixture *fixture, int x, int y) {
    int index = qualifier_index(x, y);
    fixture->elevation[index] = 15;
    fixture->filled_elevation[index] = 20;
}

static void qualifier_mark_shallow(QualificationFixture *fixture, int x, int y) {
    int index = qualifier_index(x, y);
    fixture->elevation[index] = 18;
    fixture->filled_elevation[index] = 20;
}

static int qualifier_is_basin(const QualificationFixture *fixture, int index) {
    return index >= 0 && index < QUALIFIER_COUNT &&
           fixture->filled_elevation[index] - fixture->elevation[index] >= 2 &&
           fixture->filled_elevation[index] == 20;
}

static void qualifier_route_to_outlet(QualificationFixture *fixture,
                                      int source_x, int source_y,
                                      int receiver_x, int receiver_y) {
    int source = qualifier_index(source_x, source_y);
    int receiver = qualifier_index(receiver_x, receiver_y);
    int head = 0;
    int tail = 0;
    fixture->visited[source] = 1;
    fixture->receiver[source] = receiver;
    fixture->heap[tail++] = source;
    while (head < tail) {
        int current = fixture->heap[head++];
        int x = current % QUALIFIER_W;
        int y = current / QUALIFIER_W;
        int direction;
        for (direction = 0; direction < 4; direction++) {
            int nx = x + CARDINAL_DX[direction];
            int ny = y + CARDINAL_DY[direction];
            int neighbor;
            if (nx < 0 || nx >= QUALIFIER_W || ny < 0 || ny >= QUALIFIER_H) {
                continue;
            }
            neighbor = qualifier_index(nx, ny);
            if (!qualifier_is_basin(fixture, neighbor) ||
                fixture->visited[neighbor]) continue;
            fixture->visited[neighbor] = 1;
            fixture->receiver[neighbor] = current;
            fixture->heap[tail++] = neighbor;
        }
    }
    while (tail > 0) fixture->visited[fixture->heap[--tail]] = 0;
}

static void qualifier_add_external_support(QualificationFixture *fixture,
                                           int basin_x, int basin_y,
                                           int donor_x, int donor_y) {
    int basin = qualifier_index(basin_x, basin_y);
    int donor = qualifier_index(donor_x, donor_y);
    fixture->receiver[donor] = basin;
    fixture->upstream_count[donor] = 64;
    fixture->check_flow[donor] = 128;
    fixture->upstream_count[basin] = 65;
    fixture->check_flow[basin] = 132;
}

static void mark_rectangle(QualificationFixture *fixture,
                           int left, int top, int right, int bottom) {
    int x;
    int y;
    for (y = top; y <= bottom; y++) {
        for (x = left; x <= right; x++) qualifier_mark_basin(fixture, x, y);
    }
}

static int coherent_basin_contract(FILE *file) {
    QualificationFixture fixture;
    RiverLakeQualification candidate;
    int collected;
    int accepted;
    qualifier_fixture_init(&fixture);
    mark_rectangle(&fixture, 6, 6, 8, 8);
    qualifier_route_to_outlet(&fixture, 8, 7, 9, 7);
    qualifier_add_external_support(&fixture, 6, 6, 6, 5);
    collected = river_lake_qualification_collect(
        &fixture.state, qualifier_index(7, 7), &candidate);
    accepted = collected && river_lake_qualification_accepts(&candidate);
    fprintf(file,
            "case=landform_lake_coherent collected=%d initial=%d retained=%d "
            "pruned=%d depth=%d deep=%d edges=%d outlets=%d reasons=%u ok=%d\n",
            collected, collected ? candidate.initial_area : -1,
            collected ? candidate.area : -1,
            collected ? candidate.pruned_cells : -1,
            collected ? candidate.max_depth : -1,
            collected ? candidate.deep_cells : -1,
            collected ? candidate.cardinal_edges : -1,
            collected ? candidate.outlet_receiver_count : -1,
            collected ? candidate.reject_reasons : 0u,
            accepted && candidate.initial_area == 9 && candidate.area == 9 &&
                candidate.pruned_cells == 0 && candidate.deep_cells == 9 &&
                candidate.cardinal_edges == 12 &&
                candidate.outlet_receiver_count == 1);
    return accepted && candidate.initial_area == 9 && candidate.area == 9 &&
           candidate.pruned_cells == 0 && candidate.deep_cells == 9 &&
           candidate.cardinal_edges == 12 && candidate.outlet_receiver_count == 1;
}

static int diagonal_components_contract(FILE *file) {
    QualificationFixture fixture;
    RiverLakeQualification first;
    RiverLakeQualification second;
    int first_collected;
    int second_collected;
    int first_accepted;
    int second_accepted;
    qualifier_fixture_init(&fixture);
    mark_rectangle(&fixture, 1, 1, 2, 3);
    mark_rectangle(&fixture, 3, 4, 4, 6);
    qualifier_route_to_outlet(&fixture, 2, 2, 3, 2);
    qualifier_route_to_outlet(&fixture, 4, 5, 5, 5);
    qualifier_add_external_support(&fixture, 1, 1, 1, 0);
    qualifier_add_external_support(&fixture, 3, 4, 3, 3);
    first_collected = river_lake_qualification_collect(
        &fixture.state, qualifier_index(1, 1), &first);
    first_accepted = first_collected && river_lake_qualification_accepts(&first);
    second_collected = river_lake_qualification_collect(
        &fixture.state, qualifier_index(3, 4), &second);
    second_accepted = second_collected && river_lake_qualification_accepts(&second);
    fprintf(file,
            "case=landform_lake_diagonal first_initial=%d second_initial=%d "
            "first_outlets=%d second_outlets=%d labels_distinct=%d ok=%d\n",
            first_collected ? first.initial_area : -1,
            second_collected ? second.initial_area : -1,
            first_collected ? first.outlet_receiver_count : -1,
            second_collected ? second.outlet_receiver_count : -1,
            first_collected && second_collected && first.label != second.label,
            first_accepted && second_accepted && first.initial_area == 6 &&
                second.initial_area == 6 && first.outlet_receiver_count == 1 &&
                second.outlet_receiver_count == 1 && first.label != second.label);
    return first_accepted && second_accepted && first.initial_area == 6 &&
           second.initial_area == 6 && first.outlet_receiver_count == 1 &&
           second.outlet_receiver_count == 1 && first.label != second.label;
}

static int negative_outlet_contract(FILE *file) {
    QualificationFixture fixture;
    RiverLakeQualification candidate;
    int source = qualifier_index(8, 7);
    int collected;
    int accepted;
    qualifier_fixture_init(&fixture);
    mark_rectangle(&fixture, 6, 6, 8, 8);
    qualifier_route_to_outlet(&fixture, 8, 7, 9, 7);
    fixture.receiver[source] = -1;
    qualifier_add_external_support(&fixture, 6, 6, 6, 5);
    collected = river_lake_qualification_collect(
        &fixture.state, qualifier_index(7, 7), &candidate);
    accepted = collected && river_lake_qualification_accepts(&candidate);
    fprintf(file,
            "case=landform_lake_negative_outlet collected=%d outlets=%d "
            "reasons=%u rejected=%d ok=%d\n",
            collected, collected ? candidate.outlet_receiver_count : -1,
            collected ? candidate.reject_reasons : 0u, !accepted,
            collected && !accepted &&
                (candidate.reject_reasons & RIVER_LAKE_REJECT_OUTLETS) != 0);
    return collected && !accepted &&
           (candidate.reject_reasons & RIVER_LAKE_REJECT_OUTLETS) != 0;
}

static int unsupported_basin_contract(FILE *file) {
    QualificationFixture fixture;
    RiverLakeQualification candidate;
    int collected;
    int accepted;
    qualifier_fixture_init(&fixture);
    mark_rectangle(&fixture, 6, 6, 8, 8);
    qualifier_route_to_outlet(&fixture, 8, 7, 9, 7);
    fixture.upstream_count[qualifier_index(7, 7)] = 64;
    fixture.check_flow[qualifier_index(7, 7)] = 128;
    collected = river_lake_qualification_collect(
        &fixture.state, qualifier_index(7, 7), &candidate);
    accepted = collected && river_lake_qualification_accepts(&candidate);
    fprintf(file,
            "case=landform_lake_external_support collected=%d area=%d "
            "outlets=%d internal_upstream=64 direct_external=0 reasons=%u ok=%d\n",
            collected, collected ? candidate.area : -1,
            collected ? candidate.outlet_receiver_count : -1,
            collected ? candidate.reject_reasons : 0u,
            collected && !accepted && candidate.outlet_receiver_count == 1 &&
                (candidate.reject_reasons &
                 (RIVER_LAKE_REJECT_CATCHMENT | RIVER_LAKE_REJECT_SUPPORT)) != 0);
    return collected && !accepted && candidate.outlet_receiver_count == 1 &&
           (candidate.reject_reasons &
            (RIVER_LAKE_REJECT_CATCHMENT | RIVER_LAKE_REJECT_SUPPORT)) != 0;
}

static int disconnected_deep_core_contract(FILE *file) {
    QualificationFixture fixture;
    RiverLakeQualification candidate;
    int x;
    int y;
    int collected;
    int accepted;
    qualifier_fixture_init(&fixture);
    for (y = 6; y <= 10; y++) {
        for (x = 6; x <= 10; x++) qualifier_mark_shallow(&fixture, x, y);
    }
    qualifier_mark_basin(&fixture, 6, 6);
    qualifier_mark_basin(&fixture, 10, 6);
    qualifier_mark_basin(&fixture, 6, 10);
    qualifier_mark_basin(&fixture, 10, 10);
    qualifier_route_to_outlet(&fixture, 10, 8, 11, 8);
    qualifier_add_external_support(&fixture, 8, 6, 8, 5);
    collected = river_lake_qualification_collect(
        &fixture.state, qualifier_index(8, 8), &candidate);
    accepted = collected && river_lake_qualification_accepts(&candidate);
    fprintf(file,
            "case=landform_lake_deep_core collected=%d area=%d deep_total=%d "
            "outlets=%d reasons=%u rejected=%d ok=%d\n",
            collected, collected ? candidate.area : -1,
            collected ? candidate.deep_cells : -1,
            collected ? candidate.outlet_receiver_count : -1,
            collected ? candidate.reject_reasons : 0u, !accepted,
            collected && !accepted && candidate.outlet_receiver_count == 1 &&
                (candidate.reject_reasons & RIVER_LAKE_REJECT_DEEP_CELLS) != 0);
    return collected && !accepted && candidate.outlet_receiver_count == 1 &&
           (candidate.reject_reasons & RIVER_LAKE_REJECT_DEEP_CELLS) != 0;
}

static void mark_morphology(QualificationFixture *fixture, int shape) {
    int x;
    int y;
    if (shape == 0) {
        mark_rectangle(fixture, 1, 7, 15, 8);
    } else if (shape == 1) {
        mark_rectangle(fixture, 1, 1, 2, 15);
        for (y = 1; y <= 13; y += 6) mark_rectangle(fixture, 3, y, 15, y + 1);
    } else if (shape == 2) {
        for (x = 3; x <= 13; x++) {
            qualifier_mark_basin(fixture, x, 3);
            qualifier_mark_basin(fixture, x, 13);
        }
        for (y = 4; y <= 12; y++) {
            qualifier_mark_basin(fixture, 3, y);
            qualifier_mark_basin(fixture, 13, y);
        }
    } else if (shape == 3) {
        mark_rectangle(fixture, 1, 5, 6, 10);
        mark_rectangle(fixture, 7, 7, 15, 8);
    } else if (shape == 4) {
        mark_rectangle(fixture, 1, 1, 6, 15);
        for (y = 1; y <= 13; y += 6) mark_rectangle(fixture, 7, y, 15, y + 1);
    } else {
        mark_rectangle(fixture, 1, 1, 3, 15);
        for (y = 1; y <= 13; y += 6) mark_rectangle(fixture, 4, y, 15, y + 2);
    }
}

static int morphology_rejection_contract(FILE *file, const char *label,
                                         int shape, int seed_x, int seed_y,
                                         int donor_x, int donor_y,
                                         int source_x, int source_y,
                                         int receiver_x, int receiver_y,
                                         uint32_t required_reasons) {
    QualificationFixture fixture;
    RiverLakeQualification candidate;
    int collected;
    int accepted;
    qualifier_fixture_init(&fixture);
    mark_morphology(&fixture, shape);
    qualifier_route_to_outlet(
        &fixture, source_x, source_y, receiver_x, receiver_y);
    qualifier_add_external_support(&fixture, seed_x, seed_y, donor_x, donor_y);
    collected = river_lake_qualification_collect(
        &fixture.state, qualifier_index(seed_x, seed_y), &candidate);
    accepted = collected && river_lake_qualification_accepts(&candidate);
    fprintf(file,
            "case=landform_lake_%s collected=%d initial=%d retained=%d "
            "pruned=%d outlets=%d interior=%d interior_perimeter=%d "
            "reasons=%u required=%u ok=%d\n",
            label, collected, collected ? candidate.initial_area : -1,
            collected ? candidate.area : -1,
            collected ? candidate.pruned_cells : -1,
            collected ? candidate.outlet_receiver_count : -1,
            collected ? candidate.interior_cells : -1,
            collected ? candidate.interior_perimeter_edges : -1,
            collected ? candidate.reject_reasons : 0u,
            required_reasons,
            collected && !accepted && candidate.outlet_receiver_count == 1 &&
                (candidate.reject_reasons & required_reasons) == required_reasons);
    return collected && !accepted && candidate.outlet_receiver_count == 1 &&
           (candidate.reject_reasons & required_reasons) == required_reasons;
}

int game_worldgen_lake_qualification_probe_run(FILE *file) {
    int ok;
    if (!file) return 0;
    ok = coherent_basin_contract(file);
    ok &= diagonal_components_contract(file);
    ok &= negative_outlet_contract(file);
    ok &= unsupported_basin_contract(file);
    ok &= disconnected_deep_core_contract(file);
    ok &= morphology_rejection_contract(
        file, "tendril_2wide", 0, 1, 7, 1, 6, 15, 8, 16, 8,
        RIVER_LAKE_REJECT_SHAPE);
    ok &= morphology_rejection_contract(
        file, "comb_2wide", 1, 1, 8, 0, 8, 15, 14, 16, 14,
        RIVER_LAKE_REJECT_SHAPE);
    ok &= morphology_rejection_contract(
        file, "ring_perforated", 2, 3, 3, 3, 2, 13, 13, 14, 13,
        RIVER_LAKE_REJECT_SHAPE);
    ok &= morphology_rejection_contract(
        file, "core_arm_2wide", 3, 1, 5, 1, 4, 15, 8, 16, 8,
        RIVER_LAKE_REJECT_SHAPE);
    ok &= morphology_rejection_contract(
        file, "thick_core_comb", 4, 1, 8, 0, 8, 15, 14, 16, 14,
        RIVER_LAKE_REJECT_SHAPE);
    ok &= morphology_rejection_contract(
        file, "comb_3wide", 5, 1, 8, 0, 8, 15, 14, 16, 14,
        RIVER_LAKE_REJECT_SHAPE | RIVER_LAKE_REJECT_COMPACTNESS);
    fprintf(file, "case=landform_lake_qualification_summary ok=%d\n", ok);
    return ok;
}
