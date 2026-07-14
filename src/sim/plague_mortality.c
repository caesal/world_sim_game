#include "sim/plague_mortality.h"

#include "core/game_types.h"
#include "sim/plague_rules.h"
#include "sim/plague_state.h"
#include "sim/population.h"
#include "sim/population_mortality.h"

int plague_mortality_apply_city_month(int city_id, int absolute_month) {
    PlagueModelState *model = plague_state_mutable();
    PlagueCityEpisodeState *city;
    PopulationSummary summary;
    PopulationPlagueDeaths applied;
    int requested;
    int owner;
    if (!model->episode.active || city_id < 0 || city_id >= city_count ||
        !cities[city_id].alive || cities[city_id].population <= 0) return 0;
    city = &model->cities[city_id];
    if (!city->active || city->infection_start_month > absolute_month ||
        city->recovery_month <= absolute_month) return 0;
    summary = population_city_summary(city_id);
    requested = plague_rules_monthly_deaths(summary.total,
                                             model->episode.severity,
                                             &city->mortality_carry_q32);
    if (requested <= 0) return 0;
    applied = population_apply_weighted_plague_deaths(city_id, requested);
    if (applied.total <= 0) return 0;
    population_sync_city(city_id);
    owner = cities[city_id].owner;
    if (owner >= 0 && owner < civ_count) {
        civs[owner].population = max(0, civs[owner].population - applied.total);
    }
    plague_state_record_deaths(absolute_month, city_id, owner, applied.total);
    return applied.total;
}
