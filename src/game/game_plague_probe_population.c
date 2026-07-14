#include "game/game_plague_probe_internal.h"

#include "core/game_types.h"
#include "sim/plague_disorder.h"
#include "sim/plague_mortality.h"
#include "sim/plague_state.h"
#include "sim/population.h"
#include "sim/population_display_cohorts.h"
#include "sim/population_mortality.h"

#include <stdlib.h>
#include <string.h>

static void reset_population_fixture(void) {
    memset(cities, 0, sizeof(cities));
    memset(civs, 0, sizeof(civs));
    city_count = 1;
    civ_count = 1;
    civs[0].alive = 1;
    civs[0].uid = 901;
    civs[0].population = 0;
    cities[0].alive = 1;
    cities[0].owner = 0;
    cities[0].population_ready = 1;
    plague_state_reset();
    plague_disorder_reset();
}

static void set_equal_bands(int people_per_band) {
    PopulationDisplayCohorts display;
    PopulationSummary summary;
    int band;
    int total = 0;
    memset(&summary, 0, sizeof(summary));
    memset(cities[0].population_cohorts, 0,
           sizeof(cities[0].population_cohorts));
    for (band = 0; band < POP_COHORT_COUNT; band++) {
        cities[0].population_cohorts[band].male = people_per_band / 2;
        cities[0].population_cohorts[band].female =
            people_per_band - people_per_band / 2;
        summary.cohorts[band] = cities[0].population_cohorts[band];
        total += people_per_band;
    }
    cities[0].population = total;
    civs[0].population = total;
    population_display_init_city(0);
    population_display_uniform_from_summary(&display, summary);
    population_display_replace_city_for_validation(0, &display);
}

static void begin_mortality_episode(int severity, int duration_months) {
    PlagueEpisodeState episode;
    memset(&episode, 0, sizeof(episode));
    episode.active = 1;
    episode.size = PLAGUE_SIZE_SMALL;
    episode.severity = severity;
    episode.start_month = 0;
    plague_state_begin_episode(&episode);
    plague_state_infect_city(0, 0, 0, duration_months);
}

static void check_weighted_age_allocation(PlagueProbeContext *context) {
    static const int expected[POP_COHORT_COUNT] = {30, 20, 10, 10, 10, 20, 30, 50};
    PopulationPlagueDeaths deaths;
    int band;
    int ok = 1;
    reset_population_fixture();
    set_equal_bands(100);
    deaths = population_apply_weighted_plague_deaths(0, 180);
    for (band = 0; band < POP_COHORT_COUNT; band++) {
        if (deaths.by_band[band] != expected[band]) ok = 0;
    }
    plague_probe_check(context, "mortality", "weighted_age_allocation",
        ok && deaths.total == 180 && cities[0].population == 800,
        "removed=%d weights=3,2,1,1,1,2,3,5", deaths.total);
}

static void check_age_allocation_capacity_cap(PlagueProbeContext *context) {
    PopulationPlagueDeaths deaths;
    int band;
    int emptied = 1;
    reset_population_fixture();
    set_equal_bands(100);
    deaths = population_apply_weighted_plague_deaths(0, 900);
    for (band = 0; band < POP_COHORT_COUNT; band++) {
        if (cities[0].population_cohorts[band].male != 0 ||
            cities[0].population_cohorts[band].female != 0) emptied = 0;
    }
    plague_probe_check(context, "mortality", "age_allocation_caps_at_population",
                       deaths.total == 800 && emptied,
                       "requested=900 available=800 removed=%d", deaths.total);
}

static void check_twelve_month_mortality_integration(PlagueProbeContext *context) {
    const PlagueModelState *model;
    int total_deaths = 0;
    int absolute_month;
    reset_population_fixture();
    set_equal_bands(125000);
    begin_mortality_episode(1, 13);
    for (absolute_month = 0; absolute_month < 12; absolute_month++) {
        total_deaths += plague_mortality_apply_city_month(0, absolute_month);
    }
    model = plague_state_get();
    plague_probe_check(context, "mortality", "twelve_month_q32_application",
        abs(total_deaths - 60000) <= 1 &&
        model->episode.total_deaths == total_deaths &&
        model->cities[0].episode_deaths == total_deaths &&
        plague_state_rolling_deaths(11) == total_deaths &&
        plague_mortality_apply_city_month(0, 13) == 0,
        "severity=1 population=1000000 deaths=%d expected=60000", total_deaths);
}

static void check_mortality_infection_window(PlagueProbeContext *context) {
    reset_population_fixture();
    set_equal_bands(125);
    begin_mortality_episode(10, 24);
    plague_probe_check(context, "mortality", "infection_window_is_half_open",
        plague_mortality_apply_city_month(0, -1) == 0 &&
        plague_mortality_apply_city_month(0, 24) == 0 &&
        plague_state_get()->episode.total_deaths == 0,
        "active_for_start_le_month_less_than_recovery");
}

static void check_disorder_target_and_rates(PlagueProbeContext *context) {
    int decay = -1;
    int max_current = -1;
    int max_target = -1;
    int target;
    int rise;
    int active_decline;
    int inactive_decline;
    reset_population_fixture();
    set_equal_bands(125);
    begin_mortality_episode(10, 24);
    plague_state_record_deaths(0, 0, 0, 20);
    plague_disorder_refresh_targets(0);
    target = plague_disorder_target(0);
    rise = plague_disorder_step(0, 0, &decay);
    active_decline = plague_disorder_step(0, 100, &decay);
    civs[0].disorder_plague = 30;
    plague_disorder_max_current_target(&max_current, &max_target);
    plague_state_finish_episode(1, NULL);
    plague_disorder_refresh_targets(1);
    inactive_decline = plague_disorder_step(0, 100, &decay);
    plague_probe_check(context, "disorder", "target_formula_and_bounded_steps",
        target == 84 && rise == 12 && active_decline == 94 &&
        max_current == 30 && max_target == 84 && inactive_decline == 90 &&
        decay == 10,
        "target=%d rise=%d active_decline=%d inactive_decline=%d",
        target, rise, active_decline, inactive_decline);
}

void plague_probe_run_population(PlagueProbeContext *context) {
    check_weighted_age_allocation(context);
    check_age_allocation_capacity_cap(context);
    check_twelve_month_mortality_integration(context);
    check_mortality_infection_window(context);
    check_disorder_target_and_rates(context);
}
