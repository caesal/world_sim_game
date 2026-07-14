#include "game/game_presentation_probe.h"
#include "game/game_presentation_plague_fog_probe.h"
#include "game/game_presentation_plague_probability_probe.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

#define PRESENTATION_PROBE_DIR "build/validation/presentation_probe_20260618"

int game_presentation_core_probe(FILE *summary);
int game_presentation_map_speed_probe(FILE *summary);
int game_presentation_topbar_probe(FILE *summary);
int game_presentation_layout_probe(FILE *summary);
int game_presentation_map_ocean_probe(FILE *summary);
int game_presentation_map_decision_probe(FILE *summary);
int game_presentation_diplomacy_probe(FILE *summary);
int game_presentation_regression_probe(FILE *summary);
int game_presentation_world_policy_probe(FILE *summary);
int game_presentation_world_announcement_probe(FILE *summary);
int game_presentation_plague_probe(FILE *summary);

int run_presentation_probe(void) {
    FILE *summary;
    int ok = 1;
    CreateDirectoryA("build", NULL);
    CreateDirectoryA("build/validation", NULL);
    CreateDirectoryA(PRESENTATION_PROBE_DIR, NULL);
    summary = fopen(PRESENTATION_PROBE_DIR "/summary.txt", "w");
    if (!summary) return 2;
    ok &= game_presentation_core_probe(summary);
    ok &= game_presentation_map_speed_probe(summary);
    ok &= game_presentation_topbar_probe(summary);
    ok &= game_presentation_layout_probe(summary);
    ok &= game_presentation_diplomacy_probe(summary);
    ok &= game_presentation_regression_probe(summary);
    ok &= game_presentation_map_ocean_probe(summary);
    ok &= game_presentation_map_decision_probe(summary);
    ok &= game_presentation_world_policy_probe(summary);
    ok &= game_presentation_world_announcement_probe(summary);
    ok &= game_presentation_plague_probe(summary);
    ok &= game_presentation_plague_probability_probe(summary);
    ok &= game_presentation_plague_fog_probe(summary);
    fprintf(summary, "overall_ok=%d\n", ok);
    fclose(summary);
    printf("presentation probe summary: %s\\summary.txt\n", PRESENTATION_PROBE_DIR);
    return ok ? 0 : 1;
}
