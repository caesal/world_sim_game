#ifndef WORLD_SIM_GAME_PRESENTATION_COAST_ARTIFACT_PATTERN_H
#define WORLD_SIM_GAME_PRESENTATION_COAST_ARTIFACT_PATTERN_H

void game_presentation_coast_pattern_detect(
    const unsigned char *mask, int width, int height,
    int *thin_runs, int *comb_clusters);

#endif
