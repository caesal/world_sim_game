#ifndef WORLD_SIM_GAME_PLAGUE_PROBE_INTERNAL_H
#define WORLD_SIM_GAME_PLAGUE_PROBE_INTERNAL_H

#include <stdio.h>

typedef struct {
    FILE *output;
    int checks;
    int failures;
} PlagueProbeContext;

void plague_probe_check(PlagueProbeContext *context, const char *suite,
                        const char *name, int passed, const char *format, ...);
void plague_probe_run_rules(PlagueProbeContext *context);
void plague_probe_run_probability(PlagueProbeContext *context);
void plague_probe_run_state(PlagueProbeContext *context);
void plague_probe_run_spread(PlagueProbeContext *context);
void plague_probe_run_population(PlagueProbeContext *context);
void plague_probe_run_integration(PlagueProbeContext *context);

#endif
