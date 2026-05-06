#ifndef WORLD_SIM_CIVILIZATION_METRICS_H
#define WORLD_SIM_CIVILIZATION_METRICS_H

#include "core/game_types.h"

void derive_civilization_metrics_from_traits_and_birthplace(Civilization *civ,
                                                            int aggression,
                                                            int expansion,
                                                            int defense,
                                                            int culture,
                                                            TerrainStats birth);
void apply_civilization_core_metrics(Civilization *civ,
                                     int governance, int cohesion, int production, int military,
                                     int commerce, int logistics, int innovation,
                                     TerrainStats birth, int birthplace_bonus);

#endif
