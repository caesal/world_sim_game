#ifndef WORLD_SIM_CIVILIZATION_METRICS_H
#define WORLD_SIM_CIVILIZATION_METRICS_H

#include "core/game_types.h"

void derive_civilization_metrics_from_traits_and_birthplace(Civilization *civ,
                                                            int aggression,
                                                            int expansion,
                                                            int defense,
                                                            int culture,
                                                            TerrainStats birth);

#endif
