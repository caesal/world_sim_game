#include "game/game_war_history_probe.h"

#include "game/game_war_history_model_probe.h"
#include "game/game_war_history_terminal_probe.h"

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

int run_war_history_probe(void) {
    const char *output_path = getenv("WORLD_SIM_WAR_HISTORY_PROBE_OUTPUT");
    FILE *out;
    int model_ok;
    int terminal_ok;
    if (!output_path || !output_path[0]) {
        CreateDirectoryA("logs", NULL);
        output_path = "logs/war_history_probe.txt";
    }
    out = fopen(output_path, "w");
    if (!out) return 2;
    fprintf(out,
            "probe=war_history contract=capacity3_principals_only_terminal_after_settlement\n");
    model_ok = run_war_history_model_probe_cases(out);
    terminal_ok = run_war_history_terminal_probe_cases(out);
    fprintf(out, "model_ok=%d terminal_ok=%d overall_ok=%d\n",
            model_ok, terminal_ok, model_ok && terminal_ok);
    fclose(out);
    return model_ok && terminal_ok ? 0 : 1;
}
