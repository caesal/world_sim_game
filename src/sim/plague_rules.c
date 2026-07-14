#include "sim/plague_rules.h"

#include "sim/plague_probability.h"

#include <string.h>

#define PLAGUE_FIRST_CHECK_MONTH (20 * 12)
#define PLAGUE_CHECK_INTERVAL_MONTHS (20 * 12)
#define PLAGUE_INFECTION_DURATION_MIN 24
#define PLAGUE_INFECTION_DURATION_MAX 42
#define PLAGUE_PULSE_INTERVAL_MONTHS 6
#define PLAGUE_PERSIST_MONTHS 12

static const PlagueSizeRules size_rules[] = {
    {0, 0, 0, 0, 0, 0, 0},
    {25, 4, 48, 1, 4, 55, 45},
    {55, 6, 72, 5, 8, 65, 35},
    {70, 8, 144, 9, 10, 75, 25}
};

static const uint64_t monthly_mortality_q32[10] = {
    UINT64_C(22089072), UINT64_C(25895681), UINT64_C(29739997),
    UINT64_C(33622809), UINT64_C(37544933), UINT64_C(41507210),
    UINT64_C(45510509), UINT64_C(49555728), UINT64_C(53643796),
    UINT64_C(57775672)
};

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int positive_mod(int value, int divisor) {
    int result;
    if (divisor <= 0) return 0;
    result = value % divisor;
    return result < 0 ? result + divisor : result;
}

int plague_rules_absolute_month(int year, int month) {
    if (year < 0) year = 0;
    month = clamp_int(month, 1, 12);
    return year * 12 + month - 1;
}

int plague_rules_scheduled_check_due(int absolute_month) {
    return absolute_month >= PLAGUE_FIRST_CHECK_MONTH &&
           (absolute_month - PLAGUE_FIRST_CHECK_MONTH) % PLAGUE_CHECK_INTERVAL_MONTHS == 0;
}

int plague_rules_next_scheduled_check(int absolute_month) {
    int elapsed;
    if (absolute_month < PLAGUE_FIRST_CHECK_MONTH) return PLAGUE_FIRST_CHECK_MONTH;
    elapsed = absolute_month - PLAGUE_FIRST_CHECK_MONTH;
    return PLAGUE_FIRST_CHECK_MONTH +
           (elapsed / PLAGUE_CHECK_INTERVAL_MONTHS + 1) * PLAGUE_CHECK_INTERVAL_MONTHS;
}

PlagueSize plague_rules_size_from_roll(int roll_0_to_99) {
    PlagueProbabilityDistribution defaults;
    plague_probability_defaults(&defaults);
    return plague_rules_size_from_distribution(&defaults, roll_0_to_99);
}

PlagueSize plague_rules_size_from_distribution(
    const PlagueProbabilityDistribution *probabilities, int roll_0_to_99) {
    return plague_probability_size_from_roll(probabilities, roll_0_to_99);
}

int plague_rules_size_values(PlagueSize size, PlagueSizeRules *out) {
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (size < PLAGUE_SIZE_SMALL || size > PLAGUE_SIZE_LARGE) return 0;
    *out = size_rules[size];
    return 1;
}

int plague_rules_spore_budget(PlagueSize size, int frozen_occupied_cities) {
    PlagueSizeRules rules;
    int budget;
    if (!plague_rules_size_values(size, &rules) || frozen_occupied_cities <= 0) return 0;
    budget = (frozen_occupied_cities * rules.spore_percent + 50) / 100;
    return budget < 1 ? 1 : budget;
}

int plague_rules_severity_from_roll(PlagueSize size, int roll) {
    PlagueSizeRules rules;
    int count;
    if (!plague_rules_size_values(size, &rules)) return 0;
    count = rules.severity_max - rules.severity_min + 1;
    return rules.severity_min + positive_mod(roll, count);
}

int plague_rules_infection_duration_from_roll(int roll) {
    return PLAGUE_INFECTION_DURATION_MIN +
           positive_mod(roll, PLAGUE_INFECTION_DURATION_MAX - PLAGUE_INFECTION_DURATION_MIN + 1);
}

int plague_rules_initial_pulse_count(int duration_months) {
    if (duration_months <= PLAGUE_PULSE_INTERVAL_MONTHS) return 0;
    return (duration_months - 1) / PLAGUE_PULSE_INTERVAL_MONTHS;
}

int plague_rules_pulse_is_before_recovery(int pulse_month, int recovery_month) {
    return pulse_month < recovery_month;
}

int plague_rules_persistence_legal(int recovery_month, int cap_month) {
    return recovery_month <= cap_month - PLAGUE_PERSIST_MONTHS;
}

void plague_rules_action_weights(PlagueSize size, int remaining_months,
                                 int last_active_city, int eligible_target_count,
                                 PlagueActionWeights *out) {
    PlagueSizeRules rules;
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!plague_rules_size_values(size, &rules)) return;
    out->spread = rules.spread_weight;
    out->persist = rules.persist_weight;
    if (remaining_months <= 12) out->persist += 30;
    if (last_active_city) out->persist += 20;
    if (eligible_target_count >= 3) out->spread += 15;
    out->total = out->spread + out->persist;
}

PlagueAction plague_rules_choose_action(PlagueSize size, int remaining_months,
                                        int last_active_city, int eligible_target_count,
                                        int spread_legal, int persist_legal, int roll) {
    PlagueActionWeights weights;
    int pick;
    if (!spread_legal && !persist_legal) return PLAGUE_ACTION_NONE;
    if (!spread_legal) return PLAGUE_ACTION_PERSIST;
    if (!persist_legal) return PLAGUE_ACTION_SPREAD;
    plague_rules_action_weights(size, remaining_months, last_active_city,
                                eligible_target_count, &weights);
    if (weights.total <= 0) return PLAGUE_ACTION_NONE;
    pick = positive_mod(roll, weights.total);
    return pick < weights.spread ? PLAGUE_ACTION_SPREAD : PLAGUE_ACTION_PERSIST;
}

void plague_rules_route_weights(int deep_unlocked, int land_available,
                                int shallow_available, int deep_available,
                                PlagueRouteWeights *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (deep_unlocked) {
        if (land_available) out->weight[PLAGUE_ROUTE_LAND] = 50;
        if (shallow_available) out->weight[PLAGUE_ROUTE_SHALLOW] = 30;
        if (deep_available) out->weight[PLAGUE_ROUTE_DEEP] = 20;
    } else {
        if (land_available) out->weight[PLAGUE_ROUTE_LAND] = 65;
        if (shallow_available) out->weight[PLAGUE_ROUTE_SHALLOW] = 35;
    }
    out->total = out->weight[PLAGUE_ROUTE_LAND] +
                 out->weight[PLAGUE_ROUTE_SHALLOW] +
                 out->weight[PLAGUE_ROUTE_DEEP];
}

int plague_rules_route_legal(PlagueRouteType route, int deep_unlocked) {
    if (route < PLAGUE_ROUTE_LAND || route >= PLAGUE_ROUTE_COUNT) return 0;
    return route != PLAGUE_ROUTE_DEEP || deep_unlocked;
}

PlagueRouteType plague_rules_choose_route(const PlagueRouteWeights *weights, int roll) {
    int pick;
    int route;
    if (!weights || weights->total <= 0) return PLAGUE_ROUTE_COUNT;
    pick = positive_mod(roll, weights->total);
    for (route = 0; route < PLAGUE_ROUTE_COUNT; route++) {
        if (pick < weights->weight[route]) return (PlagueRouteType)route;
        pick -= weights->weight[route];
    }
    return PLAGUE_ROUTE_COUNT;
}

int plague_rules_immunity_percent_for_duration(int episode_duration_months) {
    if (episode_duration_months < 60) return 30;
    if (episode_duration_months < 120) return 50;
    if (episode_duration_months < 240) return 80;
    return 100;
}

int plague_rules_next_immunity_tier(int duration_months, int *out_percent,
                                    int *out_months_remaining) {
    int next_percent = 100;
    int threshold_month = 0;
    if (duration_months < 0) duration_months = 0;
    if (duration_months < 60) {
        next_percent = 50;
        threshold_month = 60;
    } else if (duration_months < 120) {
        next_percent = 80;
        threshold_month = 120;
    } else if (duration_months < 240) {
        threshold_month = 240;
    }
    if (out_percent) *out_percent = next_percent;
    if (out_months_remaining) {
        *out_months_remaining = threshold_month > 0 ? threshold_month - duration_months : 0;
    }
    return threshold_month > 0;
}

int plague_rules_immunity_weight_percent(int immunity_percent) {
    if (immunity_percent >= 100) return 0;
    if (immunity_percent >= 80) return 20;
    if (immunity_percent >= 50) return 50;
    if (immunity_percent >= 30) return 70;
    return 100;
}

int plague_rules_annual_mortality_percent(int severity) {
    return clamp_int(severity, 1, 10) + 5;
}

uint64_t plague_rules_monthly_mortality_q32(int severity) {
    return monthly_mortality_q32[clamp_int(severity, 1, 10) - 1];
}

int plague_rules_monthly_deaths(int surviving_population, int severity,
                                uint64_t *carry_q32) {
    uint64_t carry = carry_q32 ? *carry_q32 : 0;
    uint64_t numerator;
    uint64_t deaths;
    if (surviving_population <= 0) {
        if (carry_q32) *carry_q32 = 0;
        return 0;
    }
    carry %= PLAGUE_MORTALITY_Q32_ONE;
    numerator = (uint64_t)surviving_population *
                plague_rules_monthly_mortality_q32(severity) + carry;
    deaths = numerator >> 32;
    if (carry_q32) *carry_q32 = numerator & (PLAGUE_MORTALITY_Q32_ONE - 1);
    if (deaths > (uint64_t)surviving_population) deaths = (uint64_t)surviving_population;
    return (int)deaths;
}
