#ifndef WORLD_SIM_GAME_WAR_HISTORY_SAVE_PROBE_INTERNAL_H
#define WORLD_SIM_GAME_WAR_HISTORY_SAVE_PROBE_INTERNAL_H

#include "sim/war_history_types.h"

#include <stdio.h>

typedef struct {
    FILE *summary;
    int checks;
    int failures;
} WarHistorySaveProbeContext;

void war_history_save_probe_check(WarHistorySaveProbeContext *context,
                                  const char *name, int passed,
                                  const char *detail_format, ...);
void war_history_save_probe_setup_civs(void);
void war_history_save_probe_make_state(WarHistorySaveState *state);
int war_history_save_probe_state_equal(const WarHistorySaveState *a,
                                       const WarHistorySaveState *b);
FILE *war_history_save_probe_file_from_bytes(const unsigned char *bytes,
                                             size_t size);
int war_history_save_probe_read_file(FILE *file, unsigned char **out_bytes,
                                     size_t *out_size);
FILE *war_history_save_probe_clone_file(FILE *source);

int game_war_history_save_probe_codec(WarHistorySaveProbeContext *context);
int game_war_history_save_probe_dynamic(WarHistorySaveProbeContext *context);

#endif
