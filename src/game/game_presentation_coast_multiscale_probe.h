#ifndef WORLD_SIM_GAME_PRESENTATION_COAST_MULTISCALE_PROBE_H
#define WORLD_SIM_GAME_PRESENTATION_COAST_MULTISCALE_PROBE_H

#include "core/render_snapshot.h"

#include <stdio.h>

int game_presentation_coast_multiscale_probe(
    FILE *summary, const RenderSnapshot *natural_snapshot);
int game_presentation_coast_multiscale_synthetic_probe(FILE *summary);

#endif
