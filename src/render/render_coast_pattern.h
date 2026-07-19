#ifndef WORLD_SIM_RENDER_COAST_PATTERN_H
#define WORLD_SIM_RENDER_COAST_PATTERN_H

void render_coast_pattern_detect(
    const unsigned char *mask, int width, int height,
    int *thin_runs, int *comb_clusters);

#endif
