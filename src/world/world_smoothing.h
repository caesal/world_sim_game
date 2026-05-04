#ifndef WORLD_SIM_WORLD_SMOOTHING_H
#define WORLD_SIM_WORLD_SMOOTHING_H

#include "core/game_types.h"

Climate world_majority_neighbor_climate(int x, int y, Climate fallback);

#endif
