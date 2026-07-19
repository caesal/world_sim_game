#ifndef WORLD_SIM_GAME_PRESENTATION_WATER_REFERENCE_PROBE_H
#define WORLD_SIM_GAME_PRESENTATION_WATER_REFERENCE_PROBE_H

#include "game/game_presentation_static_physical_artifacts.h"

int game_presentation_water_reference_probe(
    StaticPhysicalProbeCanvas *canvas, int *matches, int *exact_matches,
    int *pixels, int *unique_colors, int *max_delta);

#endif
