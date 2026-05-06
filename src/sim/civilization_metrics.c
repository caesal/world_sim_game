#include "civilization_metrics.h"

void apply_civilization_core_metrics(Civilization *civ,
                                     int governance, int cohesion, int production, int military,
                                     int commerce, int logistics, int innovation,
                                     TerrainStats birth, int birthplace_bonus) {
    int bonus = birthplace_bonus ? 1 : 0;

    civ->governance = clamp(governance + bonus * (birth.pop_capacity + birth.water) / 16, 0, 10);
    civ->cohesion = clamp(cohesion + bonus * birth.habitability / 18, 0, 10);
    civ->production = clamp(production + bonus * (birth.wood + birth.stone + birth.minerals) / 18, 0, 10);
    civ->military = clamp(military + bonus * (birth.minerals + birth.attack) / 16, 0, 10);
    civ->commerce = clamp(commerce + bonus * (birth.money + birth.water) / 16, 0, 10);
    civ->logistics = clamp(logistics + bonus * (birth.water + birth.livestock) / 18, 0, 10);
    civ->innovation = clamp(innovation + bonus * (birth.minerals + birth.money) / 20, 0, 10);
    civ->aggression = civ->military;
    civ->expansion = clamp((civ->logistics + civ->commerce) / 2, 0, 10);
    civ->defense = clamp((civ->governance + civ->cohesion) / 2, 0, 10);
    civ->culture = clamp((civ->cohesion + civ->innovation) / 2, 0, 10);
    civ->adaptation = clamp((civ->cohesion + civ->innovation + birth.habitability +
                             clamp(6 - birth.habitability, 0, 6)) / 4, 0, 10);
}

void derive_civilization_metrics_from_traits_and_birthplace(Civilization *civ,
                                                            int aggression,
                                                            int expansion,
                                                            int defense,
                                                            int culture,
                                                            TerrainStats birth) {
    apply_civilization_core_metrics(civ,
                                    (defense + culture) / 2,
                                    culture,
                                    (expansion + defense) / 2,
                                    aggression,
                                    (culture + expansion) / 2,
                                    expansion,
                                    (culture + defense) / 2,
                                    birth, 1);
}
