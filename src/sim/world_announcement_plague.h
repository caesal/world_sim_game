#ifndef WORLD_SIM_WORLD_ANNOUNCEMENT_PLAGUE_H
#define WORLD_SIM_WORLD_ANNOUNCEMENT_PLAGUE_H

#include "sim/plague_types.h"

int world_announcement_plague_emit_start(PlagueEpisodeState *episode);
int world_announcement_plague_emit_end(PlagueEpisodeState *episode,
                                       const PlagueEpisodeHistory *history);

#endif
