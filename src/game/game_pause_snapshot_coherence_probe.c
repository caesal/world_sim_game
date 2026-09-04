#include "game/game_pause_snapshot_coherence_probe.h"

#include "game/game_pause_snapshot_coherence_probe_internal.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PROBE_DIR \
    "build/v37a_pause_fix_a01/02_pause_snapshot_probe/attempt_01"

static int valid_output_path(const char *path) {
    return path && strncmp(path, "build/", 6) == 0 &&
           strlen(path) < MAX_PATH - 24 && !strstr(path, "..") &&
           !strchr(path, ':') && !strchr(path, '\\');
}

static int ensure_directory_tree(const char *path) {
    char buffer[MAX_PATH];
    char *cursor;
    if (!valid_output_path(path)) return 0;
    snprintf(buffer, sizeof(buffer), "%s", path);
    for (cursor = buffer; *cursor; cursor++) {
        if (*cursor != '/') continue;
        *cursor = '\0';
        if (buffer[0] && !CreateDirectoryA(buffer, NULL) &&
            GetLastError() != ERROR_ALREADY_EXISTS) return 0;
        *cursor = '/';
    }
    return CreateDirectoryA(buffer, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

int run_pause_snapshot_coherence_probe(void) {
    const char *probe_dir = getenv("WORLD_SIM_PAUSE_SNAPSHOT_PROBE_DIR");
    char summary_path[MAX_PATH];
    PauseProbeReport report;
    FILE *summary;
    int cases_ok;
    int restoration_ok;
    int overall_ok;

    if (!probe_dir || !probe_dir[0]) probe_dir = DEFAULT_PROBE_DIR;
    if (!ensure_directory_tree(probe_dir) ||
        snprintf(summary_path, sizeof(summary_path), "%s/summary.txt", probe_dir) >=
            (int)sizeof(summary_path)) return 2;
    if (GetFileAttributesA(summary_path) != INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "pause snapshot attempt already contains evidence: %s\n",
                probe_dir);
        return 2;
    }
    summary = fopen(summary_path, "w");
    if (!summary) return 2;
    memset(&report, 0, sizeof(report));
    report.summary = summary;
    fprintf(summary, "probe=pause_snapshot_coherence path=%s\n", probe_dir);
    cases_ok = pause_probe_run_cases(&report);
    restoration_ok = pause_probe_cleanup();
    overall_ok = cases_ok && restoration_ok && report.case_count == report.pass_count;
    fprintf(summary,
            "final_restoration_ok=%d cases=%d passed=%d overall_ok=%d\n",
            restoration_ok, report.case_count, report.pass_count, overall_ok);
    fclose(summary);
    printf("pause snapshot coherence summary: %s\n", summary_path);
    return overall_ok ? 0 : 1;
}
