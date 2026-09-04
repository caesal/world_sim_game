#ifndef WORLD_SIM_GAME_PRESENTATION_WORLDGEN_INITIAL_CIVS_PROBE_H
#define WORLD_SIM_GAME_PRESENTATION_WORLDGEN_INITIAL_CIVS_PROBE_H

#include "game/game_presentation_worldgen_controls_artifact_internal.h"

int worldgen_initial_civs_probe_run(WorldgenControlsProbeReport *report,
                                    HWND owner);
int worldgen_initial_civs_artifact_matrix(
    WorldgenControlsArtifactWriter *writer);

#endif
