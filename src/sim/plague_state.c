#include "sim/plague_state.h"

#include "sim/plague_immunity.h"
#include "sim/plague_probability.h"
#include "sim/plague_rules.h"

#include <string.h>

#define PLAGUE_PULSE_INTERVAL_MONTHS 6

static PlagueModelState model;

static int positive_mod(int value, int divisor) {
    int result;
    if (divisor <= 0) return 0;
    result = value % divisor;
    return result < 0 ? result + divisor : result;
}

static int valid_city_id(int city_id) {
    return city_id >= 0 && city_id < MAX_CITIES;
}

static int bit_is_set(const uint64_t bits[2], int index) {
    return index >= 0 && index < PLAGUE_NAME_COUNT &&
           (bits[index / 64] & (UINT64_C(1) << (index % 64))) != 0;
}

static void set_bit(uint64_t bits[2], int index) {
    if (index >= 0 && index < PLAGUE_NAME_COUNT) {
        bits[index / 64] |= UINT64_C(1) << (index % 64);
    }
}

static int set_country_bit(uint64_t bits[PLAGUE_CIV_WORD_COUNT], int civ_id) {
    uint64_t mask;
    if (civ_id < 0 || civ_id >= MAX_CIVS) return 0;
    mask = UINT64_C(1) << (civ_id % 64);
    if (bits[civ_id / 64] & mask) return 0;
    bits[civ_id / 64] |= mask;
    return 1;
}

static int used_name_count(void) {
    int i;
    int count = 0;
    for (i = 0; i < PLAGUE_NAME_COUNT; i++) {
        if (bit_is_set(model.used_name_bits, i)) count++;
    }
    return count;
}

static void clear_current_city_episode(PlagueCityEpisodeState *city) {
    if (!city) return;
    city->active = 0;
    city->ever_infected_current_episode = 0;
    city->generation = 0;
    city->infection_start_month = 0;
    city->recovery_month = 0;
    city->next_pulse_month = 0;
    city->episode_deaths = 0;
    city->mortality_carry_q32 = 0;
}

void plague_state_reset(void) {
    int i;
    memset(&model, 0, sizeof(model));
    model.initialized = 1;
    model.next_episode_id = 1;
    model.name_cycle = 1;
    plague_probability_model_reset(&model);
    for (i = 0; i < PLAGUE_DEATH_WINDOW_MONTHS; i++) {
        model.death_month_keys[i] = -1;
    }
}

const PlagueModelState *plague_state_get(void) {
    if (!model.initialized) plague_state_reset();
    return &model;
}

PlagueModelState *plague_state_mutable(void) {
    if (!model.initialized) plague_state_reset();
    return &model;
}

void plague_state_copy(PlagueModelState *out) {
    if (!out) return;
    *out = *plague_state_get();
}

int plague_state_restore(const PlagueModelState *saved) {
    if (!saved) return 0;
    if (!plague_probability_model_validate(saved)) return 0;
    if (saved->active_city_count < 0 || saved->active_city_count > MAX_CITIES) return 0;
    if (saved->ever_infected_city_count < 0 ||
        saved->ever_infected_city_count > MAX_CITIES) return 0;
    if (saved->rolling_start_count < 0 ||
        saved->rolling_start_count > PLAGUE_ROLLING_START_CAP) return 0;
    if (saved->history_count < 0 ||
        saved->history_count > PLAGUE_RECENT_HISTORY_CAP) return 0;
    if (saved->history_head < 0 ||
        saved->history_head >= PLAGUE_RECENT_HISTORY_CAP) return 0;
    model = *saved;
    model.initialized = 1;
    if (model.next_episode_id < 1) model.next_episode_id = 1;
    if (model.name_cycle < 1) model.name_cycle = 1;
    return 1;
}

int plague_state_apply_probabilities(
    const PlagueProbabilityDistribution *probabilities) {
    plague_state_get();
    return plague_probability_model_apply(&model, probabilities);
}

int plague_state_allocate_episode_id(void) {
    int id;
    plague_state_get();
    id = model.next_episode_id++;
    if (model.next_episode_id < 1) model.next_episode_id = 1;
    return id;
}

int plague_state_begin_episode(const PlagueEpisodeState *episode) {
    int i;
    if (!episode || episode->active == 0) return 0;
    plague_state_get();
    if (model.episode.active) return 0;
    for (i = 0; i < model.ever_infected_city_count; i++) {
        int city_id = model.ever_infected_city_ids[i];
        if (valid_city_id(city_id)) clear_current_city_episode(&model.cities[city_id]);
    }
    memset(model.ever_infected_flags, 0, sizeof(model.ever_infected_flags));
    model.active_city_count = 0;
    model.ever_infected_city_count = 0;
    model.episode = *episode;
    model.episode.active = 1;
    model.episode.active_city_count = 0;
    model.episode.ever_infected_city_count = 0;
    model.episode.current_affected_country_count = 0;
    model.episode.ever_affected_country_count = 0;
    model.episode.current_month_deaths = 0;
    model.episode.total_deaths = 0;
    memset(model.episode.current_country_bits, 0,
           sizeof(model.episode.current_country_bits));
    memset(model.episode.ever_country_bits, 0,
           sizeof(model.episode.ever_country_bits));
    if (model.episode.episode_id <= 0) {
        model.episode.episode_id = plague_state_allocate_episode_id();
    } else if (model.next_episode_id <= model.episode.episode_id) {
        model.next_episode_id = model.episode.episode_id + 1;
    }
    return 1;
}

int plague_state_mark_ever_infected(int city_id) {
    if (!valid_city_id(city_id)) return 0;
    plague_state_get();
    if (model.ever_infected_flags[city_id]) return 0;
    if (model.ever_infected_city_count >= MAX_CITIES) return 0;
    model.ever_infected_flags[city_id] = 1;
    model.ever_infected_city_ids[model.ever_infected_city_count++] = city_id;
    model.cities[city_id].ever_infected_current_episode = 1;
    model.episode.ever_infected_city_count = model.ever_infected_city_count;
    return 1;
}

int plague_state_infect_city(int city_id, int generation, int infection_start_month,
                             int duration_months) {
    PlagueCityEpisodeState *city;
    if (!model.episode.active || !valid_city_id(city_id) || duration_months <= 0) return 0;
    city = &model.cities[city_id];
    if (city->active) return 0;
    if (model.active_city_count >= MAX_CITIES) return 0;
    city->active = 1;
    city->generation = generation < 0 ? 0 : generation;
    city->infection_start_month = infection_start_month;
    city->recovery_month = infection_start_month + duration_months;
    city->next_pulse_month = infection_start_month + PLAGUE_PULSE_INTERVAL_MONTHS;
    city->episode_deaths = 0;
    city->mortality_carry_q32 = 0;
    model.active_city_ids[model.active_city_count++] = city_id;
    model.episode.active_city_count = model.active_city_count;
    if (city->generation > model.episode.current_generation) {
        model.episode.current_generation = city->generation;
    }
    if (city->generation > model.episode.maximum_generation_reached) {
        model.episode.maximum_generation_reached = city->generation;
    }
    plague_state_mark_ever_infected(city_id);
    return 1;
}

int plague_state_remove_active_city(int city_id) {
    int i;
    if (!valid_city_id(city_id)) return 0;
    plague_state_get();
    for (i = 0; i < model.active_city_count; i++) {
        if (model.active_city_ids[i] == city_id) {
            memmove(&model.active_city_ids[i], &model.active_city_ids[i + 1],
                    (size_t)(model.active_city_count - i - 1) * sizeof(model.active_city_ids[0]));
            model.active_city_count--;
            model.cities[city_id].active = 0;
            model.episode.active_city_count = model.active_city_count;
            model.episode.current_generation = 0;
            for (i = 0; i < model.active_city_count; i++) {
                int active_id = model.active_city_ids[i];
                if (valid_city_id(active_id) &&
                    model.cities[active_id].generation > model.episode.current_generation) {
                    model.episode.current_generation = model.cities[active_id].generation;
                }
            }
            return 1;
        }
    }
    return 0;
}

void plague_state_clear_current_countries(void) {
    plague_state_get();
    memset(model.episode.current_country_bits, 0,
           sizeof(model.episode.current_country_bits));
    model.episode.current_affected_country_count = 0;
}

int plague_state_note_current_country(int civ_id) {
    plague_state_get();
    if (!set_country_bit(model.episode.current_country_bits, civ_id)) return 0;
    model.episode.current_affected_country_count++;
    return 1;
}

int plague_state_note_ever_country(int civ_id) {
    plague_state_get();
    if (!set_country_bit(model.episode.ever_country_bits, civ_id)) return 0;
    model.episode.ever_affected_country_count++;
    return 1;
}

int plague_state_apply_episode_immunity(int episode_end_month) {
    int i;
    int changed = 0;
    int duration;
    plague_state_get();
    if (!model.episode.active) return 0;
    duration = episode_end_month - model.episode.start_month;
    if (duration < 0) duration = 0;
    for (i = 0; i < model.ever_infected_city_count; i++) {
        int city_id = model.ever_infected_city_ids[i];
        if (!valid_city_id(city_id)) continue;
        changed += plague_immunity_apply_for_episode(&model.cities[city_id], duration,
                                                     episode_end_month);
    }
    return changed;
}

void plague_state_push_history(const PlagueEpisodeHistory *history) {
    int index;
    if (!history) return;
    plague_state_get();
    if (model.history_count < PLAGUE_RECENT_HISTORY_CAP) {
        index = (model.history_head + model.history_count) % PLAGUE_RECENT_HISTORY_CAP;
        model.history_count++;
    } else {
        index = model.history_head;
        model.history_head = (model.history_head + 1) % PLAGUE_RECENT_HISTORY_CAP;
    }
    model.history[index] = *history;
}

int plague_state_finish_episode(int episode_end_month,
                                PlagueEpisodeHistory *out_history) {
    PlagueEpisodeHistory history;
    int i;
    plague_state_get();
    if (!model.episode.active) return 0;
    plague_state_apply_episode_immunity(episode_end_month);
    memset(&history, 0, sizeof(history));
    history.episode_id = model.episode.episode_id;
    history.size = model.episode.size;
    history.severity = model.episode.severity;
    history.name_id = model.episode.name_id;
    history.name_cycle = model.episode.name_cycle;
    history.origin_city_id = model.episode.origin_city_id;
    history.origin_civ_id = model.episode.origin_civ_id;
    history.origin_civ_uid = model.episode.origin_civ_uid;
    memcpy(history.origin_city_name, model.episode.origin_city_name,
           sizeof(history.origin_city_name));
    memcpy(history.origin_civ_name_en, model.episode.origin_civ_name_en,
           sizeof(history.origin_civ_name_en));
    memcpy(history.origin_civ_name_zh, model.episode.origin_civ_name_zh,
           sizeof(history.origin_civ_name_zh));
    history.origin_civ_symbol = model.episode.origin_civ_symbol;
    history.origin_civ_color = model.episode.origin_civ_color;
    history.start_month = model.episode.start_month;
    history.end_month = episode_end_month;
    history.duration_months = episode_end_month - model.episode.start_month;
    if (history.duration_months < 0) history.duration_months = 0;
    history.spores_initial = model.episode.spores_initial;
    history.spores_used = model.episode.spores_initial - model.episode.spores_remaining;
    if (history.spores_used < 0) history.spores_used = 0;
    history.infected_city_count = model.ever_infected_city_count;
    history.affected_country_count = model.episode.ever_affected_country_count;
    history.total_deaths = model.episode.total_deaths;
    plague_state_push_history(&history);
    for (i = 0; i < model.active_city_count; i++) {
        int city_id = model.active_city_ids[i];
        if (valid_city_id(city_id)) model.cities[city_id].active = 0;
    }
    model.active_city_count = 0;
    model.episode.active_city_count = 0;
    model.episode.spores_remaining = 0;
    model.episode.active = 0;
    plague_probability_model_promote_pending(&model);
    if (out_history) *out_history = history;
    return 1;
}

void plague_state_expire_rolling_starts(int absolute_month) {
    int write = 0;
    int i;
    plague_state_get();
    for (i = 0; i < model.rolling_start_count; i++) {
        int start = model.rolling_start_months[i];
        int age = absolute_month - start;
        if (age >= 0 && age < 1200) {
            model.rolling_start_months[write++] = start;
        }
    }
    model.rolling_start_count = write;
}

int plague_state_rolling_start_count(int absolute_month) {
    int i;
    int count = 0;
    plague_state_get();
    for (i = 0; i < model.rolling_start_count; i++) {
        int age = absolute_month - model.rolling_start_months[i];
        if (age >= 0 && age < 1200) count++;
    }
    return count;
}

int plague_state_can_start(int absolute_month) {
    return plague_state_rolling_start_count(absolute_month) < PLAGUE_ROLLING_START_CAP;
}

int plague_state_record_start(int absolute_month) {
    plague_state_expire_rolling_starts(absolute_month);
    if (model.rolling_start_count >= PLAGUE_ROLLING_START_CAP) return 0;
    model.rolling_start_months[model.rolling_start_count++] = absolute_month;
    return 1;
}

int plague_state_choose_unused_name(int roll, int *out_name_id, int *out_cycle) {
    int remaining;
    int pick;
    int i;
    plague_state_get();
    if (used_name_count() >= PLAGUE_NAME_COUNT) {
        memset(model.used_name_bits, 0, sizeof(model.used_name_bits));
        model.name_cycle++;
        if (model.name_cycle < 2) model.name_cycle = 2;
    }
    remaining = PLAGUE_NAME_COUNT - used_name_count();
    if (remaining <= 0) return 0;
    pick = positive_mod(roll, remaining);
    for (i = 0; i < PLAGUE_NAME_COUNT; i++) {
        if (bit_is_set(model.used_name_bits, i)) continue;
        if (pick-- == 0) {
            set_bit(model.used_name_bits, i);
            if (out_name_id) *out_name_id = i;
            if (out_cycle) *out_cycle = model.name_cycle;
            return 1;
        }
    }
    return 0;
}

int plague_state_name_is_used(int name_id) {
    plague_state_get();
    return bit_is_set(model.used_name_bits, name_id);
}

int plague_state_name_cycle(void) {
    plague_state_get();
    return model.name_cycle;
}

void plague_state_prepare_death_month(int absolute_month) {
    int slot = positive_mod(absolute_month, PLAGUE_DEATH_WINDOW_MONTHS);
    plague_state_get();
    if (model.death_month_keys[slot] == absolute_month) return;
    model.death_month_keys[slot] = absolute_month;
    model.deaths_by_month[slot] = 0;
    memset(model.deaths_by_month_civ[slot], 0,
           sizeof(model.deaths_by_month_civ[slot]));
    if (model.episode.active) model.episode.current_month_deaths = 0;
}

void plague_state_record_deaths(int absolute_month, int city_id, int civ_id,
                                int deaths) {
    int slot;
    if (deaths <= 0) return;
    plague_state_prepare_death_month(absolute_month);
    slot = positive_mod(absolute_month, PLAGUE_DEATH_WINDOW_MONTHS);
    model.deaths_by_month[slot] += deaths;
    if (civ_id >= 0 && civ_id < MAX_CIVS) {
        model.deaths_by_month_civ[slot][civ_id] += deaths;
    }
    if (valid_city_id(city_id)) model.cities[city_id].episode_deaths += deaths;
    if (model.episode.active) {
        model.episode.current_month_deaths += deaths;
        model.episode.total_deaths += deaths;
    }
}

int64_t plague_state_rolling_deaths(int absolute_month) {
    int i;
    int64_t total = 0;
    plague_state_get();
    for (i = 0; i < PLAGUE_DEATH_WINDOW_MONTHS; i++) {
        int age = absolute_month - model.death_month_keys[i];
        if (model.death_month_keys[i] >= 0 && age >= 0 &&
            age < PLAGUE_DEATH_WINDOW_MONTHS) total += model.deaths_by_month[i];
    }
    return total;
}

int64_t plague_state_rolling_deaths_for_civ(int absolute_month, int civ_id) {
    int i;
    int64_t total = 0;
    if (civ_id < 0 || civ_id >= MAX_CIVS) return 0;
    plague_state_get();
    for (i = 0; i < PLAGUE_DEATH_WINDOW_MONTHS; i++) {
        int age = absolute_month - model.death_month_keys[i];
        if (model.death_month_keys[i] >= 0 && age >= 0 &&
            age < PLAGUE_DEATH_WINDOW_MONTHS) {
            total += model.deaths_by_month_civ[i][civ_id];
        }
    }
    return total;
}

int plague_state_recent_history(int newest_offset, PlagueEpisodeHistory *out) {
    int index;
    plague_state_get();
    if (!out || newest_offset < 0 || newest_offset >= model.history_count) return 0;
    index = (model.history_head + model.history_count - 1 - newest_offset) %
            PLAGUE_RECENT_HISTORY_CAP;
    *out = model.history[index];
    return 1;
}

void plague_state_build_view(int absolute_month, PlagueStateView *out) {
    int city_id;
    int i;
    if (!out) return;
    memset(out, 0, sizeof(*out));
    plague_state_get();
    out->episode = model.episode;
    plague_probability_model_copy_view(&model, out);
    out->active_city_count = model.active_city_count;
    memcpy(out->active_city_ids, model.active_city_ids,
           (size_t)model.active_city_count * sizeof(out->active_city_ids[0]));
    for (i = 0; i < MAX_CIVS; i++) {
        if ((model.episode.current_country_bits[i / 64] &
             (UINT64_C(1) << (i % 64))) != 0) {
            out->current_country_ids[out->current_country_count++] = i;
        }
    }
    out->next_scheduled_check_month = plague_rules_next_scheduled_check(absolute_month);
    out->starts_in_rolling_window = plague_state_rolling_start_count(absolute_month);
    out->rolling_12_month_deaths = plague_state_rolling_deaths(absolute_month);
    for (city_id = 0; city_id < MAX_CITIES; city_id++) {
        int tier = plague_immunity_tier_index(
            plague_immunity_effective_percent(&model.cities[city_id], absolute_month));
        if (tier >= 0) out->immunity_city_count[tier]++;
    }
    out->recent_history_count = model.history_count < PLAGUE_VIEW_HISTORY_COUNT ?
                                model.history_count : PLAGUE_VIEW_HISTORY_COUNT;
    for (i = 0; i < out->recent_history_count; i++) {
        plague_state_recent_history(i, &out->recent_history[i]);
    }
}
