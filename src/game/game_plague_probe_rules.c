#include "game/game_plague_probe_internal.h"

#include "sim/plague_rules.h"

#include <stdint.h>

static void check_schedule(PlagueProbeContext *context) {
    int ok = plague_rules_absolute_month(0, 1) == 0 &&
             plague_rules_absolute_month(19, 12) == 239 &&
             plague_rules_absolute_month(20, 1) == 240 &&
             !plague_rules_scheduled_check_due(239) &&
             plague_rules_scheduled_check_due(240) &&
             !plague_rules_scheduled_check_due(479) &&
             plague_rules_scheduled_check_due(480) &&
             plague_rules_next_scheduled_check(0) == 240 &&
             plague_rules_next_scheduled_check(240) == 480;
    plague_probe_check(context, "rules", "scheduled_20_year_boundaries", ok,
                       "first=240 interval=240");
}

static void check_size_probability_boundaries(PlagueProbeContext *context) {
    int ok = plague_rules_size_from_roll(-1) == PLAGUE_SIZE_NONE &&
             plague_rules_size_from_roll(49) == PLAGUE_SIZE_NONE &&
             plague_rules_size_from_roll(50) == PLAGUE_SIZE_SMALL &&
             plague_rules_size_from_roll(74) == PLAGUE_SIZE_SMALL &&
             plague_rules_size_from_roll(75) == PLAGUE_SIZE_MEDIUM &&
             plague_rules_size_from_roll(89) == PLAGUE_SIZE_MEDIUM &&
             plague_rules_size_from_roll(90) == PLAGUE_SIZE_LARGE &&
             plague_rules_size_from_roll(99) == PLAGUE_SIZE_LARGE &&
             plague_rules_size_from_roll(100) == PLAGUE_SIZE_LARGE;
    plague_probe_check(context, "rules", "size_roll_boundaries", ok,
                       "none=0..49 small=50..74 medium=75..89 large=90..99");
}

static int size_values_match(PlagueSize size, int spore, int generation,
                             int cap, int severity_min, int severity_max) {
    PlagueSizeRules values;
    return plague_rules_size_values(size, &values) &&
           values.spore_percent == spore &&
           values.maximum_generation == generation &&
           values.continuous_city_cap_months == cap &&
           values.severity_min == severity_min &&
           values.severity_max == severity_max;
}

static void check_size_caps(PlagueProbeContext *context) {
    int ok = size_values_match(PLAGUE_SIZE_SMALL, 25, 4, 48, 1, 4) &&
             size_values_match(PLAGUE_SIZE_MEDIUM, 55, 6, 72, 5, 8) &&
             size_values_match(PLAGUE_SIZE_LARGE, 70, 8, 144, 9, 10) &&
             plague_rules_spore_budget(PLAGUE_SIZE_SMALL, 1) == 1 &&
             plague_rules_spore_budget(PLAGUE_SIZE_SMALL, 100) == 25 &&
             plague_rules_spore_budget(PLAGUE_SIZE_MEDIUM, 100) == 55 &&
             plague_rules_spore_budget(PLAGUE_SIZE_LARGE, 100) == 70 &&
             plague_rules_spore_budget(PLAGUE_SIZE_LARGE, 0) == 0;
    plague_probe_check(context, "rules", "size_generation_spore_caps", ok,
                       "generations=4,6,8 caps=48,72,144 spores=25,55,70pct");
}

static void check_duration_and_pulses(PlagueProbeContext *context) {
    int ok = plague_rules_infection_duration_from_roll(0) == 24 &&
             plague_rules_infection_duration_from_roll(18) == 42 &&
             plague_rules_infection_duration_from_roll(19) == 24 &&
             plague_rules_initial_pulse_count(24) == 3 &&
             plague_rules_initial_pulse_count(42) == 6 &&
             plague_rules_pulse_is_before_recovery(23, 24) &&
             !plague_rules_pulse_is_before_recovery(24, 24) &&
             plague_rules_persistence_legal(36, 48) &&
             !plague_rules_persistence_legal(37, 48);
    plague_probe_check(context, "rules", "duration_24_42_and_six_month_pulses", ok,
                       "duration=24..42 pulse_counts=3..6 persist_extension=12");
}

static void check_action_branches(PlagueProbeContext *context) {
    PlagueActionWeights base;
    PlagueActionWeights pressured;
    plague_rules_action_weights(PLAGUE_SIZE_SMALL, 20, 0, 0, &base);
    plague_rules_action_weights(PLAGUE_SIZE_SMALL, 12, 1, 3, &pressured);
    plague_probe_check(context, "rules", "action_weight_modifiers",
        base.spread == 55 && base.persist == 45 && base.total == 100 &&
        pressured.spread == 70 && pressured.persist == 95 &&
        pressured.total == 165,
        "base=55/45 modified=70/95");
    plague_probe_check(context, "rules", "action_forced_branches",
        plague_rules_choose_action(PLAGUE_SIZE_SMALL, 20, 0, 1, 1, 0, 999) ==
            PLAGUE_ACTION_SPREAD &&
        plague_rules_choose_action(PLAGUE_SIZE_SMALL, 20, 0, 0, 0, 1, 999) ==
            PLAGUE_ACTION_PERSIST &&
        plague_rules_choose_action(PLAGUE_SIZE_SMALL, 20, 0, 0, 0, 0, 0) ==
            PLAGUE_ACTION_NONE,
        "spread_only persist_only neither");
}

static void check_route_renormalization(PlagueProbeContext *context) {
    PlagueRouteWeights weights;
    int ok;
    plague_rules_route_weights(1, 1, 0, 1, &weights);
    ok = weights.weight[PLAGUE_ROUTE_LAND] == 50 &&
         weights.weight[PLAGUE_ROUTE_SHALLOW] == 0 &&
         weights.weight[PLAGUE_ROUTE_DEEP] == 20 && weights.total == 70 &&
         plague_rules_choose_route(&weights, 49) == PLAGUE_ROUTE_LAND &&
         plague_rules_choose_route(&weights, 50) == PLAGUE_ROUTE_DEEP &&
         plague_rules_choose_route(&weights, 69) == PLAGUE_ROUTE_DEEP;
    plague_rules_route_weights(0, 0, 1, 1, &weights);
    ok = ok && weights.weight[PLAGUE_ROUTE_LAND] == 0 &&
         weights.weight[PLAGUE_ROUTE_SHALLOW] == 35 &&
         weights.weight[PLAGUE_ROUTE_DEEP] == 0 && weights.total == 35 &&
         plague_rules_choose_route(&weights, 999) == PLAGUE_ROUTE_SHALLOW;
    plague_rules_route_weights(1, 0, 0, 0, &weights);
    ok = ok && weights.total == 0 &&
         plague_rules_choose_route(&weights, 0) == PLAGUE_ROUTE_COUNT &&
         plague_rules_route_legal(PLAGUE_ROUTE_LAND, 0) &&
         plague_rules_route_legal(PLAGUE_ROUTE_SHALLOW, 0) &&
         !plague_rules_route_legal(PLAGUE_ROUTE_DEEP, 0) &&
         plague_rules_route_legal(PLAGUE_ROUTE_DEEP, 1) &&
         !plague_rules_route_legal(PLAGUE_ROUTE_COUNT, 1);
    plague_probe_check(context, "rules", "route_availability_renormalizes_islands", ok,
                       "available weights retain 50/30/20 or 65/35; locked deep is illegal");
}

static int legal_candidate_count(int deep_unlocked, int land_count,
                                 int shallow_count, int deep_count) {
    int count = 0;
    if (plague_rules_route_legal(PLAGUE_ROUTE_LAND, deep_unlocked)) count += land_count;
    if (plague_rules_route_legal(PLAGUE_ROUTE_SHALLOW, deep_unlocked)) count += shallow_count;
    if (plague_rules_route_legal(PLAGUE_ROUTE_DEEP, deep_unlocked)) count += deep_count;
    return count;
}

static void check_deep_route_candidate_policy(PlagueProbeContext *context) {
    PlagueRouteWeights weights;
    PlagueActionWeights locked_modifier;
    PlagueActionWeights unlocked_modifier;
    int legal_count;
    PlagueAction action;

    legal_count = legal_candidate_count(0, 0, 0, 1);
    action = plague_rules_choose_action(PLAGUE_SIZE_SMALL, 20, 0, legal_count,
                                        legal_count > 0, 1, 0);
    plague_rules_route_weights(0, 0, 0, 1, &weights);
    plague_probe_check(context, "routes", "deep_only_before_unlock_persists",
        legal_count == 0 && action == PLAGUE_ACTION_PERSIST && weights.total == 0,
        "legal_candidates=%d action=%d deep_weight=%d total=%d",
        legal_count, action, weights.weight[PLAGUE_ROUTE_DEEP], weights.total);

    legal_count = legal_candidate_count(1, 0, 0, 1);
    action = plague_rules_choose_action(PLAGUE_SIZE_SMALL, 20, 0, legal_count,
                                        legal_count > 0, 0, 0);
    plague_rules_route_weights(1, 0, 0, 1, &weights);
    plague_probe_check(context, "routes", "deep_only_after_unlock_spreads_deep",
        legal_count == 1 && action == PLAGUE_ACTION_SPREAD &&
        weights.weight[PLAGUE_ROUTE_DEEP] == 20 && weights.total == 20 &&
        plague_rules_choose_route(&weights, 0) == PLAGUE_ROUTE_DEEP,
        "legal_candidates=%d action=%d deep_weight=%d total=%d",
        legal_count, action, weights.weight[PLAGUE_ROUTE_DEEP], weights.total);

    legal_count = legal_candidate_count(0, 1, 1, 1);
    plague_rules_route_weights(0, 1, 1, 1, &weights);
    plague_probe_check(context, "routes", "mixed_before_unlock_uses_65_35",
        legal_count == 2 && weights.weight[PLAGUE_ROUTE_LAND] == 65 &&
        weights.weight[PLAGUE_ROUTE_SHALLOW] == 35 &&
        weights.weight[PLAGUE_ROUTE_DEEP] == 0 && weights.total == 100,
        "legal_candidates=%d weights=%d/%d/%d total=%d", legal_count,
        weights.weight[PLAGUE_ROUTE_LAND], weights.weight[PLAGUE_ROUTE_SHALLOW],
        weights.weight[PLAGUE_ROUTE_DEEP], weights.total);

    legal_count = legal_candidate_count(1, 1, 1, 1);
    plague_rules_route_weights(1, 1, 1, 1, &weights);
    plague_probe_check(context, "routes", "mixed_after_unlock_uses_50_30_20",
        legal_count == 3 && weights.weight[PLAGUE_ROUTE_LAND] == 50 &&
        weights.weight[PLAGUE_ROUTE_SHALLOW] == 30 &&
        weights.weight[PLAGUE_ROUTE_DEEP] == 20 && weights.total == 100,
        "legal_candidates=%d weights=%d/%d/%d total=%d", legal_count,
        weights.weight[PLAGUE_ROUTE_LAND], weights.weight[PLAGUE_ROUTE_SHALLOW],
        weights.weight[PLAGUE_ROUTE_DEEP], weights.total);

    plague_rules_action_weights(PLAGUE_SIZE_SMALL, 20, 0,
                                legal_candidate_count(0, 1, 1, 1), &locked_modifier);
    plague_rules_action_weights(PLAGUE_SIZE_SMALL, 20, 0,
                                legal_candidate_count(1, 1, 1, 1), &unlocked_modifier);
    plague_probe_check(context, "routes", "locked_deep_excluded_from_three_target_modifier",
        locked_modifier.spread == 55 && locked_modifier.persist == 45 &&
        unlocked_modifier.spread == 70 && unlocked_modifier.persist == 45,
        "locked_eligible=2 weights=%d/%d unlocked_eligible=3 weights=%d/%d",
        locked_modifier.spread, locked_modifier.persist,
        unlocked_modifier.spread, unlocked_modifier.persist);

    legal_count = legal_candidate_count(0, 0, 0, 0);
    action = plague_rules_choose_action(PLAGUE_SIZE_SMALL, 20, 0, legal_count,
                                        legal_count > 0, 1, 999);
    plague_probe_check(context, "routes", "trapped_island_persists",
        legal_count == 0 && action == PLAGUE_ACTION_PERSIST,
        "legal_candidates=%d action=%d", legal_count, action);
}

static void check_immunity_boundaries(PlagueProbeContext *context) {
    int next_percent = 0;
    int months_remaining = 0;
    int ok = plague_rules_immunity_percent_for_duration(0) == 30 &&
             plague_rules_immunity_percent_for_duration(79) == 30 &&
             plague_rules_immunity_percent_for_duration(80) == 50 &&
             plague_rules_immunity_percent_for_duration(139) == 50 &&
             plague_rules_immunity_percent_for_duration(140) == 80 &&
             plague_rules_immunity_percent_for_duration(199) == 80 &&
             plague_rules_immunity_percent_for_duration(200) == 100 &&
             plague_rules_immunity_weight_percent(30) == 70 &&
             plague_rules_immunity_weight_percent(50) == 50 &&
             plague_rules_immunity_weight_percent(80) == 20 &&
             plague_rules_immunity_weight_percent(100) == 0;
    ok = ok && plague_rules_next_immunity_tier(79, &next_percent, &months_remaining) &&
         next_percent == 50 && months_remaining == 1;
    ok = ok && plague_rules_next_immunity_tier(80, &next_percent, &months_remaining) &&
         next_percent == 80 && months_remaining == 60;
    ok = ok && plague_rules_next_immunity_tier(139, &next_percent, &months_remaining) &&
         next_percent == 80 && months_remaining == 1;
    ok = ok && plague_rules_next_immunity_tier(140, &next_percent, &months_remaining) &&
         next_percent == 100 && months_remaining == 60;
    ok = ok && plague_rules_next_immunity_tier(199, &next_percent, &months_remaining) &&
         next_percent == 100 && months_remaining == 1;
    ok = ok && !plague_rules_next_immunity_tier(200, &next_percent, &months_remaining) &&
         next_percent == 100 && months_remaining == 0;
    plague_probe_check(context, "rules", "immunity_duration_boundaries", ok,
                       "duration thresholds=80,140,200 next-tier countdown verified");
}

static void check_mortality_table(PlagueProbeContext *context) {
    static const uint64_t expected[10] = {
        UINT64_C(22089072), UINT64_C(25895681), UINT64_C(29739997),
        UINT64_C(33622809), UINT64_C(37544933), UINT64_C(41507210),
        UINT64_C(45510509), UINT64_C(49555728), UINT64_C(53643796),
        UINT64_C(57775672)
    };
    int severity;
    int ok = 1;
    for (severity = 1; severity <= 10; severity++) {
        if (plague_rules_annual_mortality_percent(severity) != severity + 5 ||
            plague_rules_monthly_mortality_q32(severity) != expected[severity - 1]) {
            ok = 0;
        }
    }
    plague_probe_check(context, "rules", "mortality_q32_table", ok,
                       "severity=1..10 annual=6..15pct exact_q32=verified");
}

static void check_twelve_month_q32(PlagueProbeContext *context) {
    int severity;
    int ok = 1;
    for (severity = 1; severity <= 10; severity++) {
        uint64_t carry = 0;
        int population = 1000000;
        int month;
        for (month = 0; month < 12; month++) {
            population -= plague_rules_monthly_deaths(population, severity, &carry);
        }
        if (population < 1000000 - (severity + 5) * 10000 - 1 ||
            population > 1000000 - (severity + 5) * 10000 + 1) ok = 0;
    }
    plague_probe_check(context, "rules", "q32_twelve_month_compounding", ok,
                       "population=1000000 severity=1..10 tolerance=1");
}

void plague_probe_run_rules(PlagueProbeContext *context) {
    check_schedule(context);
    check_size_probability_boundaries(context);
    check_size_caps(context);
    check_duration_and_pulses(context);
    check_action_branches(context);
    check_route_renormalization(context);
    check_deep_route_candidate_policy(context);
    check_immunity_boundaries(context);
    check_mortality_table(context);
    check_twelve_month_q32(context);
}
