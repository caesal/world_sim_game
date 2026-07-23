#include "game/game_presentation_probe.h"
#include "game/game_presentation_plague_fog_probe.h"
#include "game/game_presentation_plague_probability_probe.h"
#include "game/game_presentation_ocean_surface_probe.h"
#include "game/game_presentation_coast_smoothing_probe.h"
#include "game/game_presentation_static_physical_artifacts.h"
#include "game/game_presentation_static_physical_probe.h"
#include "game/game_presentation_worldgen_contract_probe.h"
#include "game/game_presentation_worldgen_controls_probe.h"
#include "game/game_presentation_worldgen_probe.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORLDGEN_CONTROLS_ONLY_ENV \
    "WORLD_SIM_PRESENTATION_WORLDGEN_CONTROLS_ONLY"

int game_presentation_core_probe(FILE *summary);
int game_presentation_map_speed_probe(FILE *summary);
int game_presentation_topbar_probe(FILE *summary);
int game_presentation_layout_probe(FILE *summary);
int game_presentation_map_decision_probe(FILE *summary);
int game_presentation_diplomacy_probe(FILE *summary);
int game_presentation_regression_probe(FILE *summary);
int game_presentation_world_policy_probe(FILE *summary);
int game_presentation_world_announcement_probe(FILE *summary);
int game_presentation_plague_probe(FILE *summary);

static int environment_truthy(const char *name) {
    const char *value = getenv(name);
    return value && value[0] && strcmp(value, "0") != 0 &&
           _stricmp(value, "false") != 0 &&
           _stricmp(value, "no") != 0 &&
           _stricmp(value, "off") != 0;
}

int run_presentation_probe(void) {
    char summary_path[MAX_PATH];
    FILE *summary;
    int ok = 1;
    CreateDirectoryA("build", NULL);
    CreateDirectoryA("build/validation", NULL);
    CreateDirectoryA(PRESENTATION_PROBE_DEFAULT_DIR, NULL);
    if (!static_physical_probe_prepare_artifact_dir() ||
        !static_physical_probe_summary_path(summary_path, sizeof(summary_path))) return 2;
    summary = fopen(summary_path, "w");
    if (!summary) return 2;
    if (environment_truthy(WORLDGEN_CONTROLS_ONLY_ENV)) {
        ok &= game_presentation_worldgen_controls_probe(summary);
        fprintf(summary, "overall_ok=%d\n", ok);
        fclose(summary);
        printf("presentation probe summary: %s\n", summary_path);
        return ok ? 0 : 1;
    }
    ok &= game_presentation_core_probe(summary);
    ok &= game_presentation_map_speed_probe(summary);
    ok &= game_presentation_topbar_probe(summary);
    ok &= game_presentation_layout_probe(summary);
    ok &= game_presentation_diplomacy_probe(summary);
    ok &= game_presentation_regression_probe(summary);
    ok &= game_presentation_map_ocean_probe(summary);
    ok &= game_presentation_coast_smoothing_probe(summary);
    ok &= game_presentation_map_decision_probe(summary);
    ok &= game_presentation_world_policy_probe(summary);
    ok &= game_presentation_world_announcement_probe(summary);
    ok &= game_presentation_plague_probe(summary);
    ok &= game_presentation_plague_probability_probe(summary);
    ok &= game_presentation_plague_fog_probe(summary);
    ok &= game_presentation_worldgen_probe(summary);
    ok &= game_presentation_worldgen_contract_probe(summary);
    ok &= game_presentation_static_physical_probe(summary);
    ok &= game_presentation_worldgen_controls_probe(summary);
    fprintf(summary, "overall_ok=%d\n", ok);
    fclose(summary);
    printf("presentation probe summary: %s\n", summary_path);
    return ok ? 0 : 1;
}
