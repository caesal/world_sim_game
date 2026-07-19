#include "game/game_presentation_coast_artifact_pattern.h"
#include "render/render_coast_pattern.h"

void game_presentation_coast_pattern_detect(
    const unsigned char *mask, int width, int height,
    int *thin_runs, int *comb_clusters) {
    render_coast_pattern_detect(mask, width, height,
                                thin_runs, comb_clusters);
}
