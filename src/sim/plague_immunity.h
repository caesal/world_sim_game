#ifndef WORLD_SIM_PLAGUE_IMMUNITY_H
#define WORLD_SIM_PLAGUE_IMMUNITY_H

#include "sim/plague_types.h"

#define PLAGUE_IMMUNITY_DURATION_MONTHS (40 * 12)

int plague_immunity_effective_percent(const PlagueCityEpisodeState *city,
                                      int absolute_month);
int plague_immunity_candidate_weight_percent(const PlagueCityEpisodeState *city,
                                             int absolute_month);
int plague_immunity_apply(PlagueCityEpisodeState *city, int immunity_percent,
                          int episode_end_month);
int plague_immunity_apply_for_episode(PlagueCityEpisodeState *city,
                                      int episode_duration_months,
                                      int episode_end_month);
int plague_immunity_tier_index(int immunity_percent);

#endif
