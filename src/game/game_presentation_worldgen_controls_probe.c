#include "game/game_presentation_worldgen_controls_probe.h"

#include "game/game_presentation_static_physical_artifacts.h"
#include "game/game_presentation_worldgen_controls_probe_internal.h"
#include "render/worldgen_ui_assets.h"
#include "ui/ui_worldgen_command.h"
#include "ui/ui_worldgen_config_adapter.h"
#include "ui/ui_worldgen_control_state.h"
#include "ui/ui_worldgen_input.h"
#include "ui/ui_worldgen_random.h"

#include <stdarg.h>
#include <string.h>

static void record_one(FILE *file, const char *name, int ok,
                       const char *details, va_list arguments) {
    if (!file) return;
    fprintf(file, "case=%s ok=%d", name, ok);
    if (details && details[0]) {
        fputc(' ', file);
        vfprintf(file, details, arguments);
    }
    fputc('\n', file);
}

void worldgen_controls_probe_record(WorldgenControlsProbeReport *report,
                                    const char *name, int ok,
                                    const char *details, ...) {
    va_list arguments;
    va_list copy;
    if (!report || !name) return;
    va_start(arguments, details);
    va_copy(copy, arguments);
    record_one(report->main_summary, name, ok, details, arguments);
    record_one(report->phase_summary, name, ok, details, copy);
    va_end(copy);
    va_end(arguments);
    report->case_count++;
    if (!ok) report->failure_count++;
}

static void restore_interaction(const UiWorldgenControlState *saved) {
    ui_worldgen_control_state_clear_interaction();
    ui_worldgen_control_state_set_hovered(saved->interaction.hovered);
    ui_worldgen_control_state_set_focused(saved->interaction.focused);
    if (saved->interaction.dragging.kind != UI_WORLDGEN_CONTROL_NONE) {
        ui_worldgen_control_state_begin_drag(saved->interaction.dragging);
    }
    ui_worldgen_control_state_set_pressed(saved->interaction.pressed);
}

static int restore_context(const UiWorldgenEffectiveConfig *saved_config,
                           const UiWorldgenControlState *saved_state) {
    int tab;
    ui_worldgen_command_set_generate_hook_for_tests(NULL);
    ui_worldgen_command_reset_diagnostics();
    ui_worldgen_input_reset_diagnostics();
    ui_worldgen_random_validation_set_state(0);
    worldgen_ui_assets_reset_for_tests();
    if (saved_state->initialized && saved_state->has_applied_config) {
        ui_worldgen_config_write(&saved_state->applied_config);
        ui_worldgen_control_state_resync(UI_WORLDGEN_RESYNC_MARK_APPLIED);
        ui_worldgen_config_write(saved_config);
        ui_worldgen_control_state_resync(UI_WORLDGEN_RESYNC_KEEP_APPLIED);
        if (saved_state->generation_failed) {
            ui_worldgen_control_state_mark_generation_failed();
        }
    } else {
        ui_worldgen_config_write(saved_config);
        ui_worldgen_control_state_resync(UI_WORLDGEN_RESYNC_MARK_APPLIED);
    }
    for (tab = 0; tab < UI_WORLDGEN_TAB_COUNT; tab++) {
        ui_worldgen_control_state_set_scroll(
            (UiWorldgenControlTab)tab, saved_state->scroll_offsets[tab]);
    }
    if (saved_state->initialized) {
        ui_worldgen_control_state_set_tab(saved_state->tab);
        restore_interaction(saved_state);
    }
    {
        UiWorldgenEffectiveConfig restored;
        ui_worldgen_config_read(&restored);
        return ui_worldgen_config_equal(&restored, saved_config);
    }
}

int game_presentation_worldgen_controls_probe(FILE *summary) {
    const char *artifact_dir = static_physical_probe_artifact_dir();
    UiWorldgenEffectiveConfig saved_config;
    UiWorldgenControlState saved_state;
    WorldgenControlsProbeReport report;
    char phase_path[MAX_PATH];
    int ok;
    int restored;
    if (!summary || !static_physical_probe_join_path(
            phase_path, sizeof(phase_path), artifact_dir,
            "worldgen_controls_summary.txt")) return 0;
    memset(&report, 0, sizeof(report));
    report.main_summary = summary;
    report.phase_summary = fopen(phase_path, "w");
    if (!report.phase_summary) {
        fprintf(summary,
                "case=worldgen_controls_summary_open ok=0 path=%s\n",
                phase_path);
        return 0;
    }
    ui_worldgen_config_read(&saved_config);
    saved_state = *ui_worldgen_control_state_get();
    fprintf(report.phase_summary,
            "suite=worldgen_control_ui_phase1 deterministic=1\n");
    ok = worldgen_controls_probe_adapter(&report);
    ok &= worldgen_controls_probe_regions(&report);
    ok &= worldgen_controls_probe_interaction(&report);
    ok &= worldgen_controls_probe_artifacts(&report);
    ok &= worldgen_controls_probe_resources(&report);
    restored = restore_context(&saved_config, &saved_state);
    worldgen_controls_probe_record(
        &report, "worldgen_controls_context_restore", restored,
        "globals_restored=%d hooks_cleared=1 diagnostics_reset=1",
        restored);
    ok &= restored;
    ok &= report.failure_count == 0;
    fprintf(report.phase_summary,
            "suite_ok=%d cases=%d failures=%d\n",
            ok, report.case_count, report.failure_count);
    fclose(report.phase_summary);
    fprintf(summary,
            "case=worldgen_controls_phase1_suite ok=%d cases=%d failures=%d artifact=%s\n",
            ok, report.case_count, report.failure_count, phase_path);
    return ok;
}
