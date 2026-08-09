#include "game/game_decision_cache_probe.h"

#include "game/game_decision_cache_probe_lifecycle.h"
#include "game/game_decision_cache_probe_matrix.h"
#include "game/game_decision_stability_probe.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROOT_DIR "build/validation/decision_cache_monthly_coherence_20260801"
#define DEFAULT_PROBE_ROOT ROOT_DIR "/03_decision_cache_probe"

static int ensure_dir(const char *path) {
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

int run_decision_cache_probe(void) {
    const char *attempt = getenv("WORLD_SIM_DECISION_CACHE_ATTEMPT");
    const char *probe_root = getenv("WORLD_SIM_DECISION_CACHE_PROBE_ROOT");
    char probe_dir[MAX_PATH];
    char summary_path[MAX_PATH];
    char matrix_path[MAX_PATH];
    char stability_path[MAX_PATH];
    FILE *summary;
    int matrix_ok;
    int lifecycle_ok;
    int stability_ok;
    int using_default_root = 0;
    int ok;
    if (!attempt || !attempt[0]) attempt = "attempt_01";
    if (!probe_root || !probe_root[0]) {
        probe_root = DEFAULT_PROBE_ROOT;
        using_default_root = 1;
    } else if (strcmp(probe_root, DEFAULT_PROBE_ROOT) == 0) using_default_root = 1;
    if (strlen(attempt) > 48 || strstr(attempt, "..") ||
        strchr(attempt, '/') || strchr(attempt, '\\')) return 2;
    if (strlen(probe_root) > MAX_PATH - 64 || strstr(probe_root, "..") ||
        strncmp(probe_root, "build/validation/", 17) != 0 ||
        strchr(probe_root, ':') || strchr(probe_root, '\\')) return 2;
    if (snprintf(probe_dir, sizeof(probe_dir), "%s/%s", probe_root, attempt) >=
            (int)sizeof(probe_dir) ||
        snprintf(summary_path, sizeof(summary_path), "%s/summary.txt", probe_dir) >=
            (int)sizeof(summary_path) ||
        snprintf(matrix_path, sizeof(matrix_path), "%s/matrix.csv", probe_dir) >=
            (int)sizeof(matrix_path) ||
        snprintf(stability_path, sizeof(stability_path), "%s/stability_cases.csv",
                 probe_dir) >= (int)sizeof(stability_path)) return 2;
    if (!ensure_dir("build") || !ensure_dir("build/validation") ||
        (using_default_root && !ensure_dir(ROOT_DIR)) ||
        !ensure_dir(probe_root) || !ensure_dir(probe_dir)) return 2;
    if (GetFileAttributesA(summary_path) != INVALID_FILE_ATTRIBUTES ||
        GetFileAttributesA(matrix_path) != INVALID_FILE_ATTRIBUTES ||
        GetFileAttributesA(stability_path) != INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "decision cache attempt already contains evidence: %s\n", probe_dir);
        return 2;
    }
    summary = fopen(summary_path, "w");
    if (!summary) return 2;
    matrix_ok = game_decision_cache_probe_matrix(summary, probe_dir);
    lifecycle_ok = game_decision_cache_probe_lifecycle(summary);
    stability_ok = game_decision_stability_probe(summary, probe_dir);
    ok = matrix_ok && lifecycle_ok && stability_ok;
    fprintf(summary, "matrix_ok=%d lifecycle_ok=%d stability_ok=%d overall_ok=%d\n",
            matrix_ok, lifecycle_ok, stability_ok, ok);
    fclose(summary);
    printf("decision cache summary: %s\n", summary_path);
    return ok ? 0 : 1;
}
