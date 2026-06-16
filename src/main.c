#include "game/game.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int probe_flag_present(void) {
    FILE *file = fopen("probe_expansion.flag", "r");
    if (!file) return 0;
    fclose(file);
    remove("probe_expansion.flag");
    return 1;
}

int main(int argc, char **argv) {
    int no_activate = argc > 1 && argv[1] && strcmp(argv[1], "--no-activate") == 0;

    if ((argc > 1 && argv[1] && strcmp(argv[1], "--probe-expansion") == 0) ||
        getenv("WORLD_SIM_PROBE_EXPANSION") || probe_flag_present()) {
        return run_expansion_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-tech10") == 0) {
        return run_tech10_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-economy") == 0) {
        return run_economy_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-crisis") == 0) {
        return run_crisis_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-diplomacy") == 0) {
        return run_diplomacy_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-population") == 0) {
        return run_population_probe();
    }
    if (no_activate) return run_game_no_activate();
    return run_game();
}
