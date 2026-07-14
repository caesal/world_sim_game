#include "sim/plague_immunity.h"

#include "sim/plague_rules.h"

int plague_immunity_effective_percent(const PlagueCityEpisodeState *city,
                                      int absolute_month) {
    if (!city || city->immunity_percent <= 0) return 0;
    if (absolute_month >= city->immunity_expiry_month) return 0;
    return city->immunity_percent;
}

int plague_immunity_candidate_weight_percent(const PlagueCityEpisodeState *city,
                                             int absolute_month) {
    return plague_rules_immunity_weight_percent(
        plague_immunity_effective_percent(city, absolute_month));
}

int plague_immunity_apply(PlagueCityEpisodeState *city, int immunity_percent,
                          int episode_end_month) {
    int old_percent;
    int old_expiry;
    if (!city || immunity_percent <= 0) return 0;
    if (immunity_percent > 100) immunity_percent = 100;
    old_percent = city->immunity_percent;
    old_expiry = city->immunity_expiry_month;
    if (city->immunity_expiry_month <= episode_end_month) {
        city->immunity_percent = 0;
    }
    if (immunity_percent > city->immunity_percent) {
        city->immunity_percent = immunity_percent;
    }
    city->immunity_expiry_month = episode_end_month + PLAGUE_IMMUNITY_DURATION_MONTHS;
    return city->immunity_percent != old_percent || city->immunity_expiry_month != old_expiry;
}

int plague_immunity_apply_for_episode(PlagueCityEpisodeState *city,
                                      int episode_duration_months,
                                      int episode_end_month) {
    return plague_immunity_apply(
        city, plague_rules_immunity_percent_for_duration(episode_duration_months),
        episode_end_month);
}

int plague_immunity_tier_index(int immunity_percent) {
    if (immunity_percent >= 100) return 3;
    if (immunity_percent >= 80) return 2;
    if (immunity_percent >= 50) return 1;
    if (immunity_percent >= 30) return 0;
    return -1;
}
