#include "ui/ui_worldgen_command.h"

#include <string.h>

#include "core/worldgen_attempt.h"
#include "game/game_worldgen.h"
#include "ui/ui_forms.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_worldgen_control_state.h"
#include "ui/ui_worldgen_random.h"

static UiWorldgenCommandDiagnostics diagnostics;
static UiWorldgenGenerateHook validation_generate_hook;

static int run_existing_generator(HWND hwnd) {
    WorldGenAttemptDiagnostics before;
    WorldGenAttemptDiagnostics after;

    worldgen_attempt_get(&before);
    game_request_new_world_with_progress(hwnd);
    worldgen_attempt_get(&after);
    return after.attempt_id != before.attempt_id && !after.active &&
           after.success && after.stage == WORLDGEN_ATTEMPT_COMPLETE;
}

void ui_worldgen_command_initialize(void) {
    ui_worldgen_control_state_init_fresh();
}

int ui_worldgen_command_generate(HWND hwnd) {
    int success;

    diagnostics.generate_commands++;
    ui_forms_read_world_setup_controls();
    ui_worldgen_control_state_resync(UI_WORLDGEN_RESYNC_KEEP_APPLIED);
    diagnostics.generator_calls++;
    success = validation_generate_hook ? validation_generate_hook(hwnd) :
                                        run_existing_generator(hwnd);
    if (success) {
        diagnostics.successful_generations++;
        ui_worldgen_control_state_mark_applied();
    } else {
        ui_worldgen_control_state_mark_generation_failed();
    }
    ui_forms_write_world_setup_controls();
    ui_forms_layout(hwnd);
    ui_invalidate_full(hwnd);
    return success;
}

int ui_worldgen_command_dice(HWND hwnd) {
    diagnostics.dice_commands++;
    ui_worldgen_randomize_all();
    ui_worldgen_control_state_resync(UI_WORLDGEN_RESYNC_KEEP_APPLIED);
    ui_forms_write_world_setup_controls();
    ui_forms_layout(hwnd);
    ui_invalidate_side_panel(hwnd);
    return 1;
}

int ui_worldgen_command_reset(HWND hwnd) {
    diagnostics.reset_commands++;
    ui_worldgen_control_state_apply_balanced();
    ui_forms_write_world_setup_controls();
    ui_forms_layout(hwnd);
    ui_invalidate_side_panel(hwnd);
    return 1;
}

void ui_worldgen_command_resync_after_load(HWND hwnd) {
    ui_worldgen_control_state_resync(UI_WORLDGEN_RESYNC_MARK_APPLIED);
    ui_forms_write_world_setup_controls();
    ui_forms_layout(hwnd);
    ui_invalidate_side_panel(hwnd);
}

void ui_worldgen_command_get_diagnostics(UiWorldgenCommandDiagnostics *out) {
    if (out) *out = diagnostics;
}

void ui_worldgen_command_reset_diagnostics(void) {
    memset(&diagnostics, 0, sizeof(diagnostics));
}

void ui_worldgen_command_set_generate_hook_for_tests(
    UiWorldgenGenerateHook hook) {
    validation_generate_hook = hook;
}
