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
    if ((argc > 1 && argv[1] && strcmp(argv[1], "--probe-expansion") == 0) ||
        getenv("WORLD_SIM_PROBE_EXPANSION") || probe_flag_present()) {
        return run_expansion_probe();
    }
    return run_game();
}
