#ifndef WORLD_SIM_GAME_PRESENTATION_WATER_EDGE_PROBE_H
#define WORLD_SIM_GAME_PRESENTATION_WATER_EDGE_PROBE_H

#include "core/render_snapshot.h"
#include "game/game_presentation_static_physical_artifacts.h"

#include <stdio.h>

int game_presentation_water_edge_base_contract(
    FILE *summary, const StaticPhysicalProbeCanvas *base,
    const RenderSnapshot *snapshot);

#endif
