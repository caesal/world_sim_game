#ifndef WORLD_SIM_GAME_PRESENTATION_COAST_ARTIFACT_METRICS_H
#define WORLD_SIM_GAME_PRESENTATION_COAST_ARTIFACT_METRICS_H

#include "core/render_snapshot.h"
#include "game/game_presentation_static_physical_artifacts.h"
#include "ui/ui_types.h"

#include <stdint.h>

#define COAST_ARTIFACT_REJECT_TOLERANCE 2

typedef enum {
    COAST_ARTIFACT_LAYER_BASE,
    COAST_ARTIFACT_LAYER_COAST,
    COAST_ARTIFACT_LAYER_LAKE,
    COAST_ARTIFACT_LAYER_OCEAN,
    COAST_ARTIFACT_LAYER_RIVER,
    COAST_ARTIFACT_LAYER_COUNT
} CoastArtifactIsolationLayer;

typedef struct {
    int changed_pixels;
    int exact_cyan_pixels;
    int exact_green_pixels;
    int near_cyan_pixels;
    int near_green_pixels;
    int family_cyan_pixels;
    int family_green_pixels;
    int family_island_pixels;
    int family_wetland_pixels;
    int candidate_pixels;
    int candidate_land_pixels;
    int candidate_ocean_pixels;
    int candidate_lake_pixels;
    int coast_mask_leak_pixels;
    int water_land_leak_pixels;
    int palette_thin_runs;
    int palette_comb_clusters;
    int geometry_leak_pixels;
    int geometry_thin_runs;
    int geometry_comb_clusters;
    int thin_runs;
    int comb_clusters;
} CoastArtifactIsolationMetrics;

typedef struct {
    int ocean_ambiguous_cells;
    int ocean_checker_cells;
    int ocean_fractional_cells;
    int lake_ambiguous_cells;
    int lake_checker_cells;
    int lake_fractional_cells;
} CoastArtifactSaddleMetrics;

int game_presentation_coast_artifact_analyze(
    const StaticPhysicalProbeCanvas *canvas,
    const RenderSnapshot *snapshot, const unsigned char *categories,
    MapLayout layout,
    CoastArtifactIsolationLayer layer, const uint32_t *previous,
    unsigned char *mask, CoastArtifactIsolationMetrics *metrics);
int game_presentation_coast_artifact_detector_contract(
    int *comb_clusters, int *single_clusters);
int game_presentation_coast_artifact_saddle_contract(
    const RenderSnapshot *snapshot, CoastArtifactSaddleMetrics *metrics);

#endif
