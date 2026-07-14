#include "sim/plague_episode.h"

#include "core/game_types.h"
#include "sim/plague_immunity.h"
#include "sim/plague_rules.h"
#include "sim/plague_state.h"
#include "sim/simulation.h"

#include <stdio.h>
#include <string.h>

static int occupied_city(int city_id) {
    int owner;
    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive ||
        cities[city_id].population <= 0) return 0;
    owner = cities[city_id].owner;
    return owner >= 0 && owner < civ_count && civs[owner].alive;
}

static int select_origin(int absolute_month, int *out_occupied_count) {
    const PlagueModelState *model = plague_state_get();
    int total_weight = 0;
    int occupied_count = 0;
    int city_id;
    int pick;
    for (city_id = 0; city_id < city_count; city_id++) {
        if (!occupied_city(city_id)) continue;
        occupied_count++;
        total_weight += plague_immunity_candidate_weight_percent(&model->cities[city_id],
                                                                  absolute_month);
    }
    if (out_occupied_count) *out_occupied_count = occupied_count;
    if (total_weight <= 0) return -1;
    pick = rnd(total_weight);
    for (city_id = 0; city_id < city_count; city_id++) {
        int weight;
        if (!occupied_city(city_id)) continue;
        weight = plague_immunity_candidate_weight_percent(&model->cities[city_id],
                                                           absolute_month);
        if (weight <= 0) continue;
        if (pick < weight) return city_id;
        pick -= weight;
    }
    return -1;
}

static void capture_origin(PlagueEpisodeState *episode, int origin_city) {
    int origin_civ = cities[origin_city].owner;
    episode->origin_city_id = origin_city;
    episode->origin_civ_id = origin_civ;
    episode->origin_civ_uid = origin_civ >= 0 && origin_civ < civ_count ?
                               civs[origin_civ].uid : 0;
    if (origin_civ >= 0 && origin_civ < civ_count) {
        episode->origin_civ_symbol = civs[origin_civ].symbol;
        episode->origin_civ_color = civs[origin_civ].color;
    }
    snprintf(episode->origin_city_name, sizeof(episode->origin_city_name), "%s",
             cities[origin_city].name);
    snprintf(episode->origin_civ_name_en, sizeof(episode->origin_civ_name_en), "%s",
             civilization_display_name_for_language(origin_civ, 0));
    snprintf(episode->origin_civ_name_zh, sizeof(episode->origin_civ_name_zh), "%s",
             civilization_display_name_for_language(origin_civ, 1));
}

int plague_episode_try_scheduled_start(int absolute_month) {
    PlagueEpisodeState episode;
    PlagueSize size;
    int occupied_count;
    int origin_city;
    int name_id;
    int name_cycle;
    int duration;
    int owner;
    if (!plague_rules_scheduled_check_due(absolute_month)) return 0;
    if (plague_state_get()->episode.active) return 0;
    plague_state_expire_rolling_starts(absolute_month);
    if (!plague_state_can_start(absolute_month)) return 0;
    size = plague_rules_size_from_distribution(
        &plague_state_get()->effective_probabilities, rnd(100));
    if (size == PLAGUE_SIZE_NONE) return 0;
    origin_city = select_origin(absolute_month, &occupied_count);
    if (origin_city < 0 || occupied_count <= 0) return 0;
    if (!plague_state_choose_unused_name(rnd(1000000), &name_id, &name_cycle)) return 0;
    memset(&episode, 0, sizeof(episode));
    episode.active = 1;
    episode.episode_id = plague_state_allocate_episode_id();
    episode.size = size;
    episode.severity = plague_rules_severity_from_roll(size, rnd(1000000));
    episode.name_id = name_id;
    episode.name_cycle = name_cycle;
    episode.start_month = absolute_month;
    episode.frozen_occupied_cities = occupied_count;
    episode.spores_initial = plague_rules_spore_budget(size, occupied_count);
    episode.spores_remaining = episode.spores_initial;
    capture_origin(&episode, origin_city);
    if (!plague_state_begin_episode(&episode)) return 0;
    duration = plague_rules_infection_duration_from_roll(rnd(1000000));
    if (!plague_state_infect_city(origin_city, 0, absolute_month, duration)) return 0;
    owner = cities[origin_city].owner;
    plague_state_note_current_country(owner);
    plague_state_note_ever_country(owner);
    plague_state_record_start(absolute_month);
    return 1;
}

int plague_episode_rebuild_active_state(int absolute_month,
                                        PlagueEpisodeHistory *ended_history) {
    PlagueModelState *model = plague_state_mutable();
    int read;
    int write = 0;
    int current_generation = 0;
    if (!model->episode.active) return 0;
    plague_state_clear_current_countries();
    for (read = 0; read < model->active_city_count; read++) {
        int city_id = model->active_city_ids[read];
        PlagueCityEpisodeState *city;
        int owner;
        if (city_id < 0 || city_id >= city_count) continue;
        city = &model->cities[city_id];
        if (!city->active) continue;
        if (!cities[city_id].alive || cities[city_id].population <= 0 ||
            city->recovery_month <= absolute_month) {
            city->active = 0;
            continue;
        }
        model->active_city_ids[write++] = city_id;
        if (city->generation > current_generation) current_generation = city->generation;
        owner = cities[city_id].owner;
        if (owner >= 0 && owner < civ_count && civs[owner].alive) {
            plague_state_note_current_country(owner);
            plague_state_note_ever_country(owner);
        }
    }
    model->active_city_count = write;
    model->episode.active_city_count = write;
    model->episode.current_generation = current_generation;
    if (write > 0) return 0;
    return plague_state_finish_episode(absolute_month, ended_history);
}
