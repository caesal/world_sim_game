#include "sim/plague_spread.h"

#include "core/game_types.h"
#include "sim/plague_adjacency.h"
#include "sim/plague_immunity.h"
#include "sim/plague_rules.h"
#include "sim/plague_state.h"
#include "sim/sea_lanes.h"
#include "sim/technology.h"

#include <stdlib.h>
#include <string.h>

#define PLAGUE_PULSE_INTERVAL_MONTHS 6

static int absolute_month;
static int source_ids[MAX_CITIES];
static int source_count;
static int gather_cursor;
static PlaguePulseRequest requests[MAX_CITIES];
static int request_count;
static PlaguePulseRequest resolved[MAX_CITIES];
static int resolved_count;
static int commit_cursor;
static int best_epoch[MAX_CITIES];
static int best_request[MAX_CITIES];
static int touched_targets[MAX_CITIES];
static int touched_count;
static int resolution_epoch = 1;
static int candidate_epoch[MAX_CITIES];
static int candidate_generation = 1;
static PlagueSpreadStats stats;

static int valid_occupied_city(int city_id) {
    int owner;
    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive ||
        cities[city_id].population <= 0) return 0;
    owner = cities[city_id].owner;
    return owner >= 0 && owner < civ_count && civs[owner].alive;
}

static int deep_route_unlocked_between(int source_city, int target_city) {
    int source_owner;
    int target_owner;
    if (!valid_occupied_city(source_city) || !valid_occupied_city(target_city)) return 0;
    source_owner = cities[source_city].owner;
    target_owner = cities[target_city].owner;
    if (!technology_deep_sea_unlocked(source_owner)) return 0;
    return source_owner == target_owner || technology_deep_sea_unlocked(target_owner);
}

static int target_weight(const PlagueModelState *model, int city_id) {
    if (!model || !valid_occupied_city(city_id) ||
        model->cities[city_id].ever_infected_current_episode) return 0;
    return plague_immunity_candidate_weight_percent(&model->cities[city_id], absolute_month);
}

static uint32_t mix_priority(uint32_t value) {
    value ^= value >> 16;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15;
    value *= UINT32_C(0x846ca68b);
    return value ^ (value >> 16);
}

static uint32_t request_priority(int episode_id, int source_city) {
    uint32_t value = (uint32_t)episode_id * UINT32_C(0x9e3779b9);
    value ^= (uint32_t)absolute_month * UINT32_C(0x85ebca6b);
    value ^= (uint32_t)(source_city + 1) * UINT32_C(0xc2b2ae35);
    return mix_priority(value);
}

static void next_candidate_generation(void) {
    candidate_generation++;
    if (candidate_generation <= 0) {
        memset(candidate_epoch, 0, sizeof(candidate_epoch));
        candidate_generation = 1;
    }
}

static int count_candidates(const PlagueModelState *model, int source_city,
                            int deep_unlocked,
                            int counts[PLAGUE_ROUTE_COUNT],
                            int weight_totals[PLAGUE_ROUTE_COUNT]) {
    int unique_count = 0;
    int route;
    next_candidate_generation();
    memset(counts, 0, sizeof(int) * PLAGUE_ROUTE_COUNT);
    memset(weight_totals, 0, sizeof(int) * PLAGUE_ROUTE_COUNT);
    for (route = 0; route < PLAGUE_ROUTE_COUNT; route++) {
        const PlagueAdjacencyContact *contacts;
        int count;
        int i;
        if (!plague_rules_route_legal((PlagueRouteType)route, deep_unlocked)) continue;
        contacts = plague_adjacency_contacts(source_city, (PlagueRouteType)route, &count);
        for (i = 0; i < count; i++) {
            int target = contacts[i].target_city;
            int weight;
            if (route == PLAGUE_ROUTE_DEEP &&
                !deep_route_unlocked_between(source_city, target)) continue;
            stats.candidate_edges++;
            stats.candidate_edges_by_route[route]++;
            weight = target_weight(model, target);
            if (weight <= 0) continue;
            counts[route]++;
            weight_totals[route] += weight;
            if (candidate_epoch[target] != candidate_generation) {
                candidate_epoch[target] = candidate_generation;
                unique_count++;
            }
        }
    }
    return unique_count;
}

static int choose_target(const PlagueModelState *model, int source_city,
                         PlagueRouteType route, int weight_total,
                         int *out_lane_id) {
    const PlagueAdjacencyContact *contacts;
    int count;
    int pick;
    int i;
    if (out_lane_id) *out_lane_id = -1;
    if (weight_total <= 0) return -1;
    pick = rnd(weight_total);
    contacts = plague_adjacency_contacts(source_city, route, &count);
    for (i = 0; i < count; i++) {
        int weight;
        if (route == PLAGUE_ROUTE_DEEP &&
            !deep_route_unlocked_between(source_city, contacts[i].target_city)) continue;
        weight = target_weight(model, contacts[i].target_city);
        if (weight <= 0) continue;
        if (pick < weight) {
            if (out_lane_id) *out_lane_id = contacts[i].lane_id;
            return contacts[i].target_city;
        }
        pick -= weight;
    }
    return -1;
}

static void gather_source(int source_city) {
    PlagueModelState *model = plague_state_mutable();
    PlagueCityEpisodeState *source;
    PlagueSizeRules size_rules;
    PlagueAction action;
    PlagueRouteWeights route_weights;
    int counts[PLAGUE_ROUTE_COUNT];
    int weight_totals[PLAGUE_ROUTE_COUNT];
    int eligible_count;
    int deep_unlocked;
    int persist_legal;
    int spread_legal;
    int remaining;
    int lane_id = -1;
    int target = -1;
    PlagueRouteType route = PLAGUE_ROUTE_COUNT;
    if (source_city < 0 || source_city >= MAX_CITIES || !model->episode.active) return;
    source = &model->cities[source_city];
    if (!source->active || source->infection_start_month > absolute_month ||
        source->recovery_month <= absolute_month || source->next_pulse_month != absolute_month) return;
    source->next_pulse_month += PLAGUE_PULSE_INTERVAL_MONTHS;
    stats.due_pulses++;
    if (model->episode.spores_remaining <= 0 ||
        !plague_rules_size_values(model->episode.size, &size_rules)) return;
    deep_unlocked = valid_occupied_city(source_city) &&
                    technology_deep_sea_unlocked(cities[source_city].owner);
    eligible_count = count_candidates(model, source_city, deep_unlocked,
                                      counts, weight_totals);
    spread_legal = source->generation < size_rules.maximum_generation && eligible_count > 0;
    persist_legal = plague_rules_persistence_legal(
        source->recovery_month,
        source->infection_start_month + size_rules.continuous_city_cap_months);
    remaining = source->recovery_month - absolute_month;
    action = plague_rules_choose_action(model->episode.size, remaining,
        model->active_city_count == 1, eligible_count, spread_legal, persist_legal, rnd(1000000));
    if (action == PLAGUE_ACTION_SPREAD) {
        plague_rules_route_weights(deep_unlocked, counts[PLAGUE_ROUTE_LAND] > 0,
            counts[PLAGUE_ROUTE_SHALLOW] > 0, counts[PLAGUE_ROUTE_DEEP] > 0,
            &route_weights);
        route = plague_rules_choose_route(&route_weights, rnd(1000000));
        if (route >= PLAGUE_ROUTE_COUNT) return;
        target = choose_target(model, source_city, route, weight_totals[route], &lane_id);
        if (target < 0) return;
    } else if (action == PLAGUE_ACTION_PERSIST) {
        target = source_city;
        route = PLAGUE_ROUTE_LAND;
    } else {
        return;
    }
    if (request_count >= MAX_CITIES) return;
    requests[request_count++] = (PlaguePulseRequest){
        source_city, target, action, route, lane_id,
        source->generation + (action == PLAGUE_ACTION_SPREAD ? 1 : 0),
        request_priority(model->episode.episode_id, source_city)
    };
}

void plague_spread_reset(void) {
    absolute_month = 0;
    source_count = gather_cursor = request_count = resolved_count = commit_cursor = 0;
    touched_count = 0;
    resolution_epoch = candidate_generation = 1;
    memset(best_epoch, 0, sizeof(best_epoch));
    memset(candidate_epoch, 0, sizeof(candidate_epoch));
    memset(&stats, 0, sizeof(stats));
}

void plague_spread_begin_month(int month_value) {
    const PlagueModelState *model = plague_state_get();
    int i;
    absolute_month = month_value;
    source_count = 0;
    for (i = 0; i < model->active_city_count && source_count < MAX_CITIES; i++) {
        int city_id = model->active_city_ids[i];
        if (city_id < 0 || city_id >= MAX_CITIES ||
            !model->cities[city_id].active ||
            model->cities[city_id].infection_start_month > absolute_month) continue;
        source_ids[source_count++] = city_id;
    }
    gather_cursor = request_count = resolved_count = commit_cursor = 0;
    memset(&stats, 0, sizeof(stats));
    stats.source_count = source_count;
    plague_adjacency_refresh();
}

int plague_spread_gather_step(int source_budget) {
    int processed = 0;
    if (source_budget < 1) source_budget = 1;
    while (gather_cursor < source_count && processed < source_budget) {
        gather_source(source_ids[gather_cursor++]);
        processed++;
    }
    stats.pending_requests = request_count;
    return gather_cursor >= source_count;
}

static int better_request(const PlaguePulseRequest *candidate,
                          const PlaguePulseRequest *current) {
    if (candidate->proposed_generation != current->proposed_generation) {
        return candidate->proposed_generation < current->proposed_generation;
    }
    if (candidate->priority != current->priority) return candidate->priority < current->priority;
    return candidate->source_city_id < current->source_city_id;
}

static int compare_priority(const void *left, const void *right) {
    const PlaguePulseRequest *a = (const PlaguePulseRequest *)left;
    const PlaguePulseRequest *b = (const PlaguePulseRequest *)right;
    if (a->priority < b->priority) return -1;
    if (a->priority > b->priority) return 1;
    return a->source_city_id - b->source_city_id;
}

int plague_spread_prepare_commits(void) {
    int i;
    resolution_epoch++;
    if (resolution_epoch <= 0) {
        memset(best_epoch, 0, sizeof(best_epoch));
        resolution_epoch = 1;
    }
    touched_count = resolved_count = commit_cursor = 0;
    for (i = 0; i < request_count; i++) {
        int target = requests[i].target_city_id;
        if (requests[i].action == PLAGUE_ACTION_PERSIST) {
            resolved[resolved_count++] = requests[i];
            continue;
        }
        if (target < 0 || target >= MAX_CITIES) continue;
        if (best_epoch[target] != resolution_epoch) {
            best_epoch[target] = resolution_epoch;
            best_request[target] = i;
            touched_targets[touched_count++] = target;
        } else if (better_request(&requests[i], &requests[best_request[target]])) {
            best_request[target] = i;
        }
    }
    for (i = 0; i < touched_count && resolved_count < MAX_CITIES; i++) {
        resolved[resolved_count++] = requests[best_request[touched_targets[i]]];
    }
    stats.deduplicated_requests = request_count - resolved_count;
    qsort(resolved, (size_t)resolved_count, sizeof(resolved[0]), compare_priority);
    return resolved_count;
}

static int commit_request(const PlaguePulseRequest *request) {
    PlagueModelState *model = plague_state_mutable();
    PlagueSizeRules rules;
    int success = 0;
    if (!request || model->episode.spores_remaining <= 0 ||
        !plague_rules_size_values(model->episode.size, &rules)) return 0;
    if (request->action == PLAGUE_ACTION_PERSIST) {
        PlagueCityEpisodeState *city;
        int cap;
        if (request->source_city_id < 0 || request->source_city_id >= MAX_CITIES) return 0;
        city = &model->cities[request->source_city_id];
        cap = city->infection_start_month + rules.continuous_city_cap_months;
        if (city->active && plague_rules_persistence_legal(city->recovery_month, cap)) {
            city->recovery_month += 12;
            stats.committed_persistence++;
            success = 1;
        }
    } else if (request->action == PLAGUE_ACTION_SPREAD &&
               (request->route_type != PLAGUE_ROUTE_DEEP ||
                deep_route_unlocked_between(request->source_city_id,
                                            request->target_city_id)) &&
               target_weight(model, request->target_city_id) > 0) {
        int duration = plague_rules_infection_duration_from_roll(rnd(19));
        if (plague_state_infect_city(request->target_city_id, request->proposed_generation,
                                    absolute_month, duration)) {
            int owner = cities[request->target_city_id].owner;
            plague_state_note_current_country(owner);
            plague_state_note_ever_country(owner);
            if (request->lane_id >= 0) {
                sea_lanes_add_exposure(request->lane_id, model->episode.severity);
            }
            stats.committed_infections++;
            success = 1;
        }
    }
    if (success) model->episode.spores_remaining--;
    return success;
}

int plague_spread_commit_step(int request_budget) {
    int processed = 0;
    if (request_budget < 1) request_budget = 1;
    while (commit_cursor < resolved_count && processed < request_budget) {
        if (plague_state_get()->episode.spores_remaining <= 0) {
            commit_cursor = resolved_count;
            break;
        }
        commit_request(&resolved[commit_cursor++]);
        processed++;
    }
    return commit_cursor >= resolved_count;
}

int plague_spread_gather_done(void) { return gather_cursor >= source_count; }
int plague_spread_commit_done(void) { return commit_cursor >= resolved_count; }
void plague_spread_stats(PlagueSpreadStats *out) { if (out) *out = stats; }
