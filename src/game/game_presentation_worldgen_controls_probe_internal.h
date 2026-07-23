#ifndef WORLD_SIM_GAME_PRESENTATION_WORLDGEN_CONTROLS_PROBE_INTERNAL_H
#define WORLD_SIM_GAME_PRESENTATION_WORLDGEN_CONTROLS_PROBE_INTERNAL_H

#include <stdio.h>

typedef struct {
    FILE *main_summary;
    FILE *phase_summary;
    int case_count;
    int failure_count;
} WorldgenControlsProbeReport;

void worldgen_controls_probe_record(WorldgenControlsProbeReport *report,
                                    const char *name, int ok,
                                    const char *details, ...);

int worldgen_controls_probe_adapter(WorldgenControlsProbeReport *report);
int worldgen_controls_probe_regions(WorldgenControlsProbeReport *report);
int worldgen_controls_probe_interaction(WorldgenControlsProbeReport *report);
int worldgen_controls_probe_artifacts(WorldgenControlsProbeReport *report);
int worldgen_controls_probe_resources(WorldgenControlsProbeReport *report);

#endif
