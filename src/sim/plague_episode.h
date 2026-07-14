#ifndef WORLD_SIM_PLAGUE_EPISODE_H
#define WORLD_SIM_PLAGUE_EPISODE_H

#include "sim/plague_types.h"

int plague_episode_try_scheduled_start(int absolute_month);
int plague_episode_rebuild_active_state(int absolute_month,
                                        PlagueEpisodeHistory *ended_history);

#endif
