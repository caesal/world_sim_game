#include "game/game_worldgen_coast_semantic_fixture.h"

#include "world/world_gen_context.h"
#include "world/world_gen_land_mask.h"

#include <stdlib.h>
#include <string.h>

typedef enum {
    COAST_FIXTURE_LATTICE,
    COAST_FIXTURE_COMB,
    COAST_FIXTURE_MESH,
    COAST_FIXTURE_TENDRIL,
    COAST_FIXTURE_CLEAR
} CoastFixtureExpectation;

typedef struct {
    const char *label;
    const char *const *rows;
    int height;
    CoastFixtureExpectation expectation;
} CoastFixture;

static int allocate_measure_fields(WorldGenContext *context,
                                   int width, int height) {
    size_t count = (size_t)width * (size_t)height;
    memset(context, 0, sizeof(*context));
    context->width = width;
    context->height = height;
    context->tile_count = width * height;
    context->land_mask = (uint8_t *)calloc(count, sizeof(*context->land_mask));
    context->coastal_lowland_hint = (uint8_t *)calloc(
        count, sizeof(*context->coastal_lowland_hint));
    context->scratch_a = (int32_t *)calloc(count, sizeof(*context->scratch_a));
    context->scratch_b = (int32_t *)calloc(count, sizeof(*context->scratch_b));
    context->topological_order = (int32_t *)calloc(
        count, sizeof(*context->topological_order));
    return context->land_mask && context->coastal_lowland_hint &&
           context->scratch_a && context->scratch_b && context->topological_order;
}

static void release_measure_fields(WorldGenContext *context) {
    if (!context) return;
    free(context->land_mask);
    free(context->coastal_lowland_hint);
    free(context->scratch_a);
    free(context->scratch_b);
    free(context->topological_order);
    memset(context, 0, sizeof(*context));
}

static int decode_cell(char value, uint8_t *land, int32_t *marker) {
    if (value == 'L' || value == '#' || value == 'X') *land = 1;
    else if (value == 'W' || value == '.' || value == 'x') *land = 0;
    else return 0;
    if (value == 'L') *marker = 2;
    else if (value == 'W') *marker = 1;
    else if (value == 'X' || value == 'x') *marker = 3;
    else *marker = 0;
    return 1;
}

static int measure_fixture(const CoastFixture *fixture,
                           WorldGenLandMaskDiagnostics *out) {
    WorldGenContext context;
    int width;
    int land = 0;
    int x;
    int y;
    if (!fixture || !fixture->rows || fixture->height <= 0 || !out) return 0;
    width = (int)strlen(fixture->rows[0]);
    if (width <= 0 || !allocate_measure_fields(&context, width, fixture->height)) {
        release_measure_fields(&context);
        return 0;
    }
    for (y = 0; y < fixture->height; y++) {
        if ((int)strlen(fixture->rows[y]) != width) {
            release_measure_fields(&context);
            return 0;
        }
        for (x = 0; x < width; x++) {
            int index = y * width + x;
            if (!decode_cell(fixture->rows[y][x], &context.land_mask[index],
                             &context.scratch_a[index])) {
                release_measure_fields(&context);
                return 0;
            }
            context.coastal_lowland_hint[index] = context.land_mask[index];
            land += context.land_mask[index] != 0;
        }
    }
    context.land_mask_diagnostics.target_land_tiles = land;
    world_gen_land_mask_measure(&context);
    *out = context.land_mask_diagnostics;
    release_measure_fields(&context);
    return 1;
}

static int clear_metrics(const WorldGenLandMaskDiagnostics *metrics) {
    return metrics && metrics->target_drift == 0 &&
           metrics->lattice_cells == 0 && metrics->comb_cells == 0 &&
           metrics->mesh_cells == 0 && metrics->tendril_cells == 0 &&
           metrics->threshold_lattice_cells == 0 &&
           metrics->threshold_comb_cells == 0 &&
           metrics->threshold_mesh_cells == 0 &&
           metrics->threshold_tendril_cells == 0 &&
           metrics->topology_errors == 0;
}

static int expectation_met(CoastFixtureExpectation expectation,
                           const WorldGenLandMaskDiagnostics *metrics) {
    if (!metrics || metrics->target_drift != 0) return 0;
    if (expectation == COAST_FIXTURE_LATTICE)
        return metrics->lattice_cells > 0 && metrics->threshold_lattice_cells > 0;
    if (expectation == COAST_FIXTURE_COMB)
        return metrics->comb_cells > 0 && metrics->threshold_comb_cells > 0;
    if (expectation == COAST_FIXTURE_MESH)
        return metrics->mesh_cells > 0 && metrics->threshold_mesh_cells > 0;
    if (expectation == COAST_FIXTURE_TENDRIL)
        return metrics->tendril_cells > 0 && metrics->threshold_tendril_cells > 0;
    return clear_metrics(metrics) && metrics->land_components == 1 &&
           metrics->water_components == 1;
}

static int run_fixture(FILE *file, const CoastFixture *fixture) {
    WorldGenLandMaskDiagnostics metrics = {0};
    int measured = measure_fixture(fixture, &metrics);
    int ok = measured && expectation_met(fixture->expectation, &metrics);
    fprintf(file,
            "case=coast_semantic_fixture label=%s measured=%d lattice=%d/%d "
            "comb=%d/%d mesh=%d/%d tendril=%d/%d topology=%d "
            "components=%d/%d drift=%d ok=%d\n",
            fixture->label, measured, metrics.lattice_cells,
            metrics.threshold_lattice_cells, metrics.comb_cells,
            metrics.threshold_comb_cells, metrics.mesh_cells,
            metrics.threshold_mesh_cells, metrics.tendril_cells,
            metrics.threshold_tendril_cells, metrics.topology_errors,
            metrics.land_components, metrics.water_components,
            metrics.target_drift, ok);
    return ok;
}

int game_worldgen_coast_semantic_fixture_run(FILE *file) {
    static const char *const lattice[] = {
        "WWWWWWWWWWW", "WWWWWWWWWWW", "WWLWWWWWWWW",
        "WWWLWWWWWWW", "WWWWLWWWWWW", "WWWWWLWWWWW",
        "WWWWWWLWWWW", "WWWWWWWLWWW", "WWWWWWWWLWW",
        "WWWWWWWWWWW", "WWWWWWWWWWW"
    };
    static const char *const comb[] = {
        "LWLWWWW", "LWLWLWW", "WWWWLLW", "WWLLWLW",
        "WLLLLLW", "WWLWLWW", "LLWLLLL"
    };
    static const char *const mesh[] = {
        "WWWWWWWWW", "WWLWLWWWW", "WWWLWLWWW", "WWLWLWWWW",
        "WWWLWLWWW", "WWWWWWWWW", "WWWWWWWWW"
    };
    static const char *const tendril[] = {
        "WWWWWWW", "WWWWWWW", "WWLLWWW", "WWWWWWW", "WWWWWWW"
    };
    static const char *const straight_coast[] = {
        "LLLLLLLLL", "LLLLLLLLL", "LLLLLLLLL", "WWWWWWWWW",
        "WWWWWWWWW", "WWWWWWWWW", "WWWWWWWWW"
    };
    static const char *const compact_island[] = {
        "WWWWWWWWW", "WWWWWWWWW", "WWWLLLWWW", "WWWLLLWWW",
        "WWWLLLWWW", "WWWWWWWWW", "WWWWWWWWW"
    };
    static const char *const marker_three_ignored[] = {
        ".......", ".......", "..XX...", ".......", "......."
    };
    static const CoastFixture fixtures[] = {
        {"lattice_positive", lattice, 11, COAST_FIXTURE_LATTICE},
        {"comb_positive", comb, 7, COAST_FIXTURE_COMB},
        {"mesh_positive", mesh, 7, COAST_FIXTURE_MESH},
        {"tendril_positive", tendril, 5, COAST_FIXTURE_TENDRIL},
        {"straight_coast_negative", straight_coast, 7, COAST_FIXTURE_CLEAR},
        {"compact_island_negative", compact_island, 7, COAST_FIXTURE_CLEAR},
        {"marker_three_ignored", marker_three_ignored, 5, COAST_FIXTURE_CLEAR}
    };
    int ok = file != NULL;
    int index;
    if (!file) return 0;
    for (index = 0; index < (int)(sizeof(fixtures) / sizeof(fixtures[0])); index++)
        ok &= run_fixture(file, &fixtures[index]);
    fprintf(file, "case=coast_semantic_fixture_contract ok=%d\n", ok);
    return ok;
}
