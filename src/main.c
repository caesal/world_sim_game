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

    if (argc > 1 && argv[1] &&
        strcmp(argv[1], "--worldgen-climate-calibration-worker") == 0) {
        return run_worldgen_climate_calibration_worker(argc, argv);
    }
    if (argc > 1 && argv[1] &&
        strcmp(argv[1], "--probe-worldgen-climate-calibration") == 0) {
        return run_worldgen_climate_calibration_probe();
    }
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
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-collapse-colors") == 0) {
        return run_collapse_color_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-crisis") == 0) {
        return run_crisis_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-diplomacy") == 0) {
        return run_diplomacy_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-presentation") == 0) {
        return run_presentation_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-worldgen") == 0) {
        return run_worldgen_probe();
    }
    if (argc > 1 && argv[1] &&
        strcmp(argv[1], "--probe-worldgen-aridity-formula") == 0) {
        return run_worldgen_aridity_formula_probe();
    }
    if (argc > 1 && argv[1] &&
        strcmp(argv[1], "--probe-worldgen-aridity-smoke") == 0) {
        return run_worldgen_aridity_smoke_probe();
    }
    if (argc > 1 && argv[1] &&
        strcmp(argv[1], "--probe-worldgen-aridity-matrix") == 0) {
        return run_worldgen_aridity_matrix_probe();
    }
    if (argc > 1 && argv[1] &&
        strcmp(argv[1], "--probe-worldgen-aridity-response-pilot") == 0) {
        return run_worldgen_aridity_response_pilot_probe();
    }
    if (argc > 1 && argv[1] &&
        strcmp(argv[1],
               "--probe-worldgen-aridity-response-projection") == 0) {
        return run_worldgen_aridity_response_projection_probe();
    }
    if (argc > 1 && argv[1] &&
        strcmp(argv[1],
               "--probe-worldgen-aridity-diminishing-response") == 0) {
        return run_worldgen_aridity_diminishing_response_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-worldgen-coast") == 0) {
        return run_worldgen_coast_threshold_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-expansion-perf") == 0) {
        return run_expansion_perf_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-decision-equivalence") == 0) {
        return run_decision_equivalence_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-decision-cache") == 0) {
        return run_decision_cache_probe();
    }
    if (argc > 1 && argv[1] &&
        strcmp(argv[1], "--probe-pause-snapshot-coherence") == 0) {
        return run_pause_snapshot_coherence_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-decision-topology") == 0) {
        return run_decision_topology_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-war-history-save") == 0) {
        return run_war_history_save_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-military-alliance") == 0) {
        return run_military_alliance_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-war-history") == 0) {
        return run_war_history_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-population") == 0) {
        return run_population_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-plague-baseline") == 0) {
        return run_plague_baseline_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-plague-model") == 0) {
        return run_plague_model_probe();
    }
    if (argc > 1 && argv[1] && strcmp(argv[1], "--probe-plague-performance") == 0) {
        return run_plague_performance_probe();
    }
    if (no_activate) return run_game_no_activate();
    return run_game();
}
