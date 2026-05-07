#include "disorder.h"

#include "core/game_state.h"
#include "sim/population.h"

void disorder_update_month(int civ_id, int resource_score) {
    Civilization *civ;
    int pressure;
    int pressure_disorder;
    int scarcity_disorder;
    int recovery;
    int delta;

    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return;
    civ = &civs[civ_id];
    pressure = population_pressure_for_civ(civ_id);
    pressure_disorder = clamp((pressure - 85) / 3, 0, 45);
    scarcity_disorder = clamp(34 - resource_score, 0, 34);
    civ->disorder_resource = clamp(pressure_disorder + scarcity_disorder, 0, 100);
    civ->disorder_plague = clamp(civ->disorder_plague - 1, 0, 100);
    civ->disorder_migration = clamp(civ->disorder_migration - 1, 0, 100);
    civ->disorder_stability = clamp(civ->disorder_stability - 1, 0, 100);
    recovery = civ->governance / 2 + civ->cohesion / 2 + (pressure < 65 && resource_score > 30 ? 2 : 0);
    delta = civ->disorder_resource / 18 + civ->disorder_plague / 24 +
            civ->disorder_migration / 26 + civ->disorder_stability / 28 - recovery;
    civ->disorder = clamp(civ->disorder + delta, 0, 100);
}

void disorder_add_war_deaths(int civ_id, int deaths) {
    if (civ_id < 0 || civ_id >= civ_count || deaths <= 0) return;
    civs[civ_id].disorder_stability = clamp(civs[civ_id].disorder_stability + deaths / 2000 + 1, 0, 100);
    civs[civ_id].disorder = clamp(civs[civ_id].disorder + deaths / 3000 + 1, 0, 100);
}

void disorder_add_plague_deaths(int civ_id, int deaths) {
    if (civ_id < 0 || civ_id >= civ_count || deaths <= 0) return;
    civs[civ_id].disorder_plague = clamp(civs[civ_id].disorder_plague + deaths / 2500 + 1, 0, 100);
    civs[civ_id].disorder = clamp(civs[civ_id].disorder + deaths / 4000 + 1, 0, 100);
}
