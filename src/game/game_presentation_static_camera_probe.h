#ifndef WORLD_SIM_GAME_PRESENTATION_STATIC_CAMERA_PROBE_H
#define WORLD_SIM_GAME_PRESENTATION_STATIC_CAMERA_PROBE_H

#include "core/render_snapshot.h"
#include "game/game_presentation_static_physical_artifacts.h"

#include <stdio.h>

int game_presentation_static_camera_probe(
    FILE *summary, StaticPhysicalProbeCanvas *canvas,
    const RenderSnapshot *snapshot);

#endif
