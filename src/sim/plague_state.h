#ifndef WORLD_SIM_PLAGUE_STATE_H
#define WORLD_SIM_PLAGUE_STATE_H

#include "sim/plague_types.h"

void plague_state_reset(void);
const PlagueModelState *plague_state_get(void);
PlagueModelState *plague_state_mutable(void);
void plague_state_copy(PlagueModelState *out);
int plague_state_restore(const PlagueModelState *saved);
int plague_state_apply_probabilities(
    const PlagueProbabilityDistribution *probabilities);

int plague_state_allocate_episode_id(void);
int plague_state_begin_episode(const PlagueEpisodeState *episode);
int plague_state_infect_city(int city_id, int generation, int infection_start_month,
                             int duration_months);
int plague_state_remove_active_city(int city_id);
int plague_state_mark_ever_infected(int city_id);
void plague_state_clear_current_countries(void);
int plague_state_note_current_country(int civ_id);
int plague_state_note_ever_country(int civ_id);
int plague_state_apply_episode_immunity(int episode_end_month);
int plague_state_finish_episode(int episode_end_month,
                                PlagueEpisodeHistory *out_history);

void plague_state_expire_rolling_starts(int absolute_month);
int plague_state_rolling_start_count(int absolute_month);
int plague_state_can_start(int absolute_month);
int plague_state_record_start(int absolute_month);

int plague_state_choose_unused_name(int roll, int *out_name_id, int *out_cycle);
int plague_state_name_is_used(int name_id);
int plague_state_name_cycle(void);

void plague_state_prepare_death_month(int absolute_month);
void plague_state_record_deaths(int absolute_month, int city_id, int civ_id,
                                int deaths);
int64_t plague_state_rolling_deaths(int absolute_month);
int64_t plague_state_rolling_deaths_for_civ(int absolute_month, int civ_id);

void plague_state_push_history(const PlagueEpisodeHistory *history);
int plague_state_recent_history(int newest_offset, PlagueEpisodeHistory *out);
void plague_state_build_view(int absolute_month, PlagueStateView *out);

#endif
