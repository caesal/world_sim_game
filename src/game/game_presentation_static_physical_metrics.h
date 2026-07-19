#ifndef WORLD_SIM_GAME_PRESENTATION_STATIC_PHYSICAL_METRICS_H
#define WORLD_SIM_GAME_PRESENTATION_STATIC_PHYSICAL_METRICS_H

#include "core/render_snapshot.h"
#include "game/game_presentation_static_physical_artifacts.h"
#include "ui/ui_layout.h"

typedef struct {
    int eligible;
    int recognized;
    int shaft_recognized;
    int head_left_recognized;
    int head_right_recognized;
    int core_sample_hits;
    int halo_sample_hits;
    int sampled_points;
    int composite_width_min;
    int composite_width_max;
    int composite_width_samples;
    int lod;
    uint64_t hash;
} StaticPhysicalWindVisualMetrics;

typedef struct {
    int fixture_civs;
    int fixture_cities;
    int source_rebuilds_after_first;
    int source_rebuilds_after_cycle;
    int placement_rebuilds_after_first;
    int placement_rebuilds_after_cycle;
    int placement_pool_hits;
    int placement_pool_stores;
    int candidates;
    int drawn;
} StaticPhysicalLabelReuseMetrics;

void static_physical_probe_analyze_wind(
    const StaticPhysicalProbeCanvas *canvas, const RenderSnapshot *snapshot,
    RECT client, MapLayout layout, StaticPhysicalWindVisualMetrics *out);
int static_physical_probe_wind_style_contract(void);
int static_physical_probe_label_reuse(HDC hdc, RECT client,
                                      StaticPhysicalLabelReuseMetrics *out);

#endif
