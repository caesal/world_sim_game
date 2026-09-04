#include "game/game_plague_probe_internal.h"

#include "core/game_types.h"
#include "data/plague_names.h"
#include "sim/plague_episode.h"
#include "sim/plague_immunity.h"
#include "sim/plague_rules.h"
#include "sim/plague_state.h"

#include <stdlib.h>
#include <string.h>

static void begin_episode(PlagueSize size, int severity, int start_month,
                          int spores) {
    PlagueEpisodeState episode;
    memset(&episode, 0, sizeof(episode));
    episode.active = 1;
    episode.size = size;
    episode.severity = severity;
    episode.start_month = start_month;
    episode.spores_initial = spores;
    episode.spores_remaining = spores;
    plague_state_begin_episode(&episode);
}

static void check_rolling_start_cap(PlagueProbeContext *context) {
    int ok;
    plague_state_reset();
    ok = plague_state_record_start(0) && plague_state_record_start(240) &&
         plague_state_record_start(480) &&
         plague_state_rolling_start_count(1199) == 3 &&
         !plague_state_can_start(1199) && !plague_state_record_start(1199) &&
         plague_state_rolling_start_count(1200) == 2 &&
         plague_state_can_start(1200) && plague_state_record_start(1200) &&
         plague_state_rolling_start_count(1200) == 3;
    plague_probe_check(context, "state", "rolling_three_per_hundred_year_cap", ok,
                       "window=1200 months exact oldest expiry at age=1200");
}

static void check_active_start_skip(PlagueProbeContext *context) {
    const PlagueModelState *model;
    int episode_id;
    int spores;
    plague_state_reset();
    begin_episode(PLAGUE_SIZE_SMALL, 2, 0, 4);
    model = plague_state_get();
    episode_id = model->episode.episode_id;
    spores = model->episode.spores_remaining;
    plague_probe_check(context, "episode", "active_episode_skips_scheduled_start",
        plague_episode_try_scheduled_start(240) == 0 &&
        plague_state_get()->episode.episode_id == episode_id &&
        plague_state_get()->episode.spores_remaining == spores &&
        plague_state_rolling_start_count(240) == 0,
        "episode_id=%d spores=%d", episode_id, spores);
}

static void check_zero_origin_skip(PlagueProbeContext *context) {
    int seed;
    int ok = 1;
    memset(cities, 0, sizeof(cities));
    memset(civs, 0, sizeof(civs));
    city_count = 0;
    civ_count = 0;
    for (seed = 0; seed < 64; seed++) {
        srand((unsigned int)seed);
        plague_state_reset();
        if (plague_episode_try_scheduled_start(240) != 0 ||
            plague_state_get()->episode.active ||
            plague_state_rolling_start_count(240) != 0) ok = 0;
    }
    plague_probe_check(context, "episode", "zero_occupied_origin_never_starts", ok,
                       "deterministic_seed_sweep=64");
}

static void setup_occupied_cities(int count) {
    int i;
    memset(cities, 0, sizeof(cities));
    memset(civs, 0, sizeof(civs));
    civ_count = 1;
    city_count = count;
    civs[0].alive = 1;
    civs[0].uid = 701;
    civs[0].population = count * 1000;
    snprintf(civs[0].name, sizeof(civs[0].name), "Probe Civ");
    for (i = 0; i < count; i++) {
        cities[i].alive = 1;
        cities[i].owner = 0;
        cities[i].population = 1000;
        cities[i].x = i;
        cities[i].y = 0;
        snprintf(cities[i].name, sizeof(cities[i].name), "Probe City %d", i);
    }
}

static void check_frozen_origin_budget(PlagueProbeContext *context) {
    const PlagueModelState *model = NULL;
    int seed;
    int started = 0;
    int initial_spores = 0;
    setup_occupied_cities(2);
    for (seed = 0; seed < 256 && !started; seed++) {
        srand((unsigned int)seed);
        plague_state_reset();
        started = plague_episode_try_scheduled_start(240);
    }
    if (started) {
        model = plague_state_get();
        initial_spores = model->episode.spores_initial;
        cities[2].alive = 1;
        cities[2].owner = 0;
        cities[2].population = 1000;
        city_count = 3;
    }
    plague_probe_check(context, "episode", "occupied_count_and_spores_freeze_at_start",
        started && model && model->episode.frozen_occupied_cities == 2 &&
        initial_spores == plague_rules_spore_budget(model->episode.size, 2) &&
        model->episode.frozen_occupied_cities == 2 &&
        model->episode.spores_initial == initial_spores,
        "seed=%d frozen=%d spores=%d", started ? seed - 1 : -1,
        model ? model->episode.frozen_occupied_cities : -1, initial_spores);
}

static void check_immunity_expiry(PlagueProbeContext *context) {
    PlagueCityEpisodeState city;
    int ok;
    memset(&city, 0, sizeof(city));
    ok = plague_immunity_apply(&city, 50, 100) &&
         city.immunity_expiry_month == 580 &&
         plague_immunity_effective_percent(&city, 579) == 50 &&
         plague_immunity_candidate_weight_percent(&city, 579) == 50 &&
         plague_immunity_apply_for_episode(&city, 79, 200) &&
         city.immunity_percent == 50 && city.immunity_expiry_month == 680 &&
         plague_immunity_effective_percent(&city, 679) == 50 &&
         plague_immunity_candidate_weight_percent(&city, 679) == 50 &&
         plague_immunity_effective_percent(&city, 680) == 0 &&
         plague_immunity_candidate_weight_percent(&city, 680) == 100 &&
         plague_immunity_apply_for_episode(&city, 200, 680) &&
         city.immunity_percent == 100 && city.immunity_expiry_month == 1160 &&
         plague_immunity_tier_index(30) == 0 &&
         plague_immunity_tier_index(50) == 1 &&
         plague_immunity_tier_index(80) == 2 &&
         plague_immunity_tier_index(100) == 3;
    plague_probe_check(context, "immunity", "absolute_expiry_and_tiers", ok,
                       "duration=480 expiry_is_exclusive max_and_refresh_preserved");
}

static void check_episode_wide_immunity(PlagueProbeContext *context) {
    const PlagueModelState *model;
    PlagueEpisodeHistory history;
    int ok;
    plague_state_reset();
    begin_episode(PLAGUE_SIZE_MEDIUM, 5, 20, 3);
    ok = plague_state_infect_city(0, 0, 20, 24) &&
         plague_state_infect_city(1, 1, 80, 24) &&
         plague_state_infect_city(2, 2, 150, 24) &&
         plague_state_remove_active_city(0) &&
         plague_state_remove_active_city(1) &&
         plague_state_finish_episode(160, &history);
    model = plague_state_get();
    ok = ok && history.duration_months == 140 &&
         history.infected_city_count == 3 &&
         model->cities[0].immunity_percent == 80 &&
         model->cities[1].immunity_percent == 80 &&
         model->cities[2].immunity_percent == 80 &&
         model->cities[0].immunity_expiry_month == 640 &&
         model->cities[1].immunity_expiry_month == 640 &&
         model->cities[2].immunity_expiry_month == 640;
    plague_probe_check(context, "immunity", "episode_wide_total_duration", ok,
                       "duration=140 cities=3 tier=80 expiry=640");
}

static void check_name_catalog_and_reuse(PlagueProbeContext *context) {
    unsigned char seen[PLAGUE_NAME_COUNT];
    char english[128];
    char chinese[128];
    int i;
    int name_id = -1;
    int cycle = -1;
    int unique = 1;
    memset(seen, 0, sizeof(seen));
    plague_state_reset();
    for (i = 0; i < PLAGUE_NAME_COUNT; i++) {
        if (!plague_state_choose_unused_name(0, &name_id, &cycle) ||
            name_id < 0 || name_id >= PLAGUE_NAME_COUNT || seen[name_id] || cycle != 1) {
            unique = 0;
            break;
        }
        seen[name_id] = 1;
    }
    if (!plague_state_choose_unused_name(0, &name_id, &cycle) ||
        name_id != 0 || cycle != 2 || plague_state_name_cycle() != 2) unique = 0;
    plague_probe_check(context, "names", "one_hundred_unique_then_roman_reuse", unique &&
        plague_names_count() == PLAGUE_NAME_COUNT,
        "catalog=%d reused_id=%d cycle=%d", plague_names_count(), name_id, cycle);
    english[0] = chinese[0] = '\0';
    plague_names_format(name_id, 2, 0, english, sizeof(english));
    plague_names_format(name_id, 2, 1, chinese, sizeof(chinese));
    plague_probe_check(context, "names", "localized_roman_suffix",
        strstr(english, " II") != NULL && strlen(chinese) >= 2 &&
        strcmp(chinese + strlen(chinese) - 2, "II") == 0,
        "english_suffix=space_II chinese_suffix=II");
}

static void check_death_ring_boundaries(PlagueProbeContext *context) {
    plague_state_reset();
    begin_episode(PLAGUE_SIZE_SMALL, 1, 0, 1);
    plague_state_record_deaths(0, 0, 0, 5);
    plague_state_record_deaths(11, 0, 0, 7);
    plague_probe_check(context, "state", "rolling_death_ring_exact_window",
        plague_state_rolling_deaths(11) == 12 &&
        plague_state_rolling_deaths_for_civ(11, 0) == 12 &&
        plague_state_rolling_deaths(12) == 7 &&
        plague_state_rolling_deaths_for_civ(12, 0) == 7,
        "window=12 months oldest_expires_at_age=12");
}

void plague_probe_run_state(PlagueProbeContext *context) {
    check_rolling_start_cap(context);
    check_active_start_skip(context);
    check_zero_origin_skip(context);
    check_frozen_origin_budget(context);
    check_immunity_expiry(context);
    check_episode_wide_immunity(context);
    check_name_catalog_and_reuse(context);
    check_death_ring_boundaries(context);
}
