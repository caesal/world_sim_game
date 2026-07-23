#ifndef WORLD_SIM_UI_WORLDGEN_COMMAND_H
#define WORLD_SIM_UI_WORLDGEN_COMMAND_H

#include <windows.h>

typedef int (*UiWorldgenGenerateHook)(HWND hwnd);

typedef struct {
    unsigned int generate_commands;
    unsigned int generator_calls;
    unsigned int successful_generations;
    unsigned int dice_commands;
    unsigned int reset_commands;
} UiWorldgenCommandDiagnostics;

void ui_worldgen_command_initialize(void);
int ui_worldgen_command_generate(HWND hwnd);
int ui_worldgen_command_dice(HWND hwnd);
int ui_worldgen_command_reset(HWND hwnd);
void ui_worldgen_command_resync_after_load(HWND hwnd);

void ui_worldgen_command_get_diagnostics(UiWorldgenCommandDiagnostics *out);
void ui_worldgen_command_reset_diagnostics(void);
void ui_worldgen_command_set_generate_hook_for_tests(
    UiWorldgenGenerateHook hook);

#endif
