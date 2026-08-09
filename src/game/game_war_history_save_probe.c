#include "game/game_war_history_save_probe.h"

#include "game/game_war_history_save_probe_internal.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PHASE_ROOT "build/validation/decision_stability_war_ocean_20260801/phase_02_war_history"
#define PROBE_ROOT PHASE_ROOT "/01_war_history_save_probe"

static int ensure_directory(const char *path) {
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static int attempt_name_valid(const char *attempt) {
    return attempt && attempt[0] && strlen(attempt) <= 48 &&
           !strstr(attempt, "..") && !strchr(attempt, '/') &&
           !strchr(attempt, '\\') && !strchr(attempt, ':');
}

int run_war_history_save_probe(void) {
    const char *attempt = getenv("WORLD_SIM_WAR_HISTORY_SAVE_ATTEMPT");
    WarHistorySaveProbeContext context;
    char attempt_directory[MAX_PATH];
    char summary_path[MAX_PATH];
    int codec_ok;
    int dynamic_ok;
    int overall_ok;
    if (!attempt || !attempt[0]) attempt = "attempt_01";
    if (!attempt_name_valid(attempt) ||
        snprintf(attempt_directory, sizeof(attempt_directory), "%s/%s",
                 PROBE_ROOT, attempt) >= (int)sizeof(attempt_directory) ||
        snprintf(summary_path, sizeof(summary_path), "%s/summary.txt",
                 attempt_directory) >= (int)sizeof(summary_path)) return 2;
    if (!ensure_directory("build") || !ensure_directory("build/validation") ||
        !ensure_directory("build/validation/decision_stability_war_ocean_20260801") ||
        !ensure_directory(PHASE_ROOT) || !ensure_directory(PROBE_ROOT) ||
        !ensure_directory(attempt_directory)) return 2;
    if (GetFileAttributesA(summary_path) != INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "war-history save attempt already contains evidence: %s\n",
                attempt_directory);
        return 2;
    }
    memset(&context, 0, sizeof(context));
    context.summary = fopen(summary_path, "w");
    if (!context.summary) return 2;
    fprintf(context.summary, "probe=war_history_save map_version=21\n");
    codec_ok = game_war_history_save_probe_codec(&context);
    dynamic_ok = game_war_history_save_probe_dynamic(&context);
    overall_ok = codec_ok && dynamic_ok && context.failures == 0;
    fprintf(context.summary,
            "codec_ok=%d dynamic_ok=%d checks=%d failures=%d overall_ok=%d\n",
            codec_ok, dynamic_ok, context.checks, context.failures, overall_ok);
    fclose(context.summary);
    printf("war-history save summary: %s\n", summary_path);
    return overall_ok ? 0 : 1;
}
