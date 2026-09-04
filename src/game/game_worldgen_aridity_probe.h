#ifndef WORLD_SIM_GAME_WORLDGEN_ARIDITY_PROBE_H
#define WORLD_SIM_GAME_WORLDGEN_ARIDITY_PROBE_H

#include <stdio.h>

int game_worldgen_aridity_formula_probe_run(FILE *file);
int run_worldgen_aridity_formula_probe(void);
int run_worldgen_aridity_smoke_probe(void);
int run_worldgen_aridity_matrix_probe(void);
int run_worldgen_aridity_response_pilot_probe(void);
int run_worldgen_aridity_response_projection_probe(void);
int run_worldgen_aridity_diminishing_response_probe(void);

#endif
