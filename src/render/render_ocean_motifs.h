#ifndef WORLD_SIM_RENDER_OCEAN_MOTIFS_H
#define WORLD_SIM_RENDER_OCEAN_MOTIFS_H

#include <windows.h>

enum {
    OCEAN_MOTIF_WAVE = 0,
    OCEAN_MOTIF_SERPENT,
    OCEAN_MOTIF_WHALE,
    OCEAN_MOTIF_TENTACLE,
    OCEAN_MOTIF_BEAST,
    OCEAN_MOTIF_SHIP,
    OCEAN_MOTIF_WRECK,
    OCEAN_MOTIF_FLYING_FISH,
    OCEAN_MOTIF_WHIRLPOOL,
    OCEAN_MOTIF_WIND,
    OCEAN_MOTIF_SPOUT,
    OCEAN_MOTIF_COUNT
};

void ocean_motif_draw(HDC hdc, int type, int cx, int cy, int size,
                      COLORREF ink, int variant);
void ocean_motif_draw_wave_cluster(HDC hdc, int x, int y, int size,
                                   COLORREF ink, int variant);

#endif
