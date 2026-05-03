#include "civilization_metrics.h"

void derive_civilization_metrics_from_traits_and_birthplace(Civilization *civ,
                                                            int aggression,
                                                            int expansion,
                                                            int defense,
                                                            int culture,
                                                            TerrainStats birth) {
    civ->governance = clamp((defense + culture) / 2 + birth.pop_capacity / 4 + birth.water / 6, 0, 10);
    civ->cohesion = clamp(culture + birth.habitability / 5, 0, 10);
    civ->production = clamp((expansion + defense) / 2 + (birth.wood + birth.stone + birth.minerals) / 8, 0, 10);
    civ->military = clamp(aggression + (birth.minerals + birth.livestock) / 8 + birth.attack / 2, 0, 10);
    civ->commerce = clamp((culture + expansion) / 2 + (birth.money + birth.water) / 5, 0, 10);
    civ->logistics = clamp(expansion + (birth.water + birth.livestock) / 7, 0, 10);
    civ->innovation = clamp((culture + defense) / 2 + (birth.minerals + birth.money) / 8, 0, 10);
    civ->adaptation = clamp((culture + expansion + birth.habitability + clamp(6 - birth.habitability, 0, 6)) / 4, 0, 10);
}
