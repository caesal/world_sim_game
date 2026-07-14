#include "sim/plague_disorder.h"

#include "core/game_types.h"
#include "sim/plague_state.h"

#include <string.h>

static int targets[MAX_CIVS];

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

void plague_disorder_reset(void) {
    memset(targets, 0, sizeof(targets));
}

void plague_disorder_refresh_targets(int absolute_month) {
    const PlagueModelState *model = plague_state_get();
    long long infected_population[MAX_CIVS];
    int i;
    memset(targets, 0, sizeof(targets));
    memset(infected_population, 0, sizeof(infected_population));
    for (i = 0; model->episode.active && i < model->active_city_count; i++) {
        int city_id = model->active_city_ids[i];
        int owner;
        if (city_id < 0 || city_id >= city_count || !model->cities[city_id].active ||
            model->cities[city_id].infection_start_month > absolute_month ||
            !cities[city_id].alive || cities[city_id].population <= 0) continue;
        owner = cities[city_id].owner;
        if (owner >= 0 && owner < civ_count && civs[owner].alive) {
            infected_population[owner] += cities[city_id].population;
        }
    }
    for (i = 0; i < civ_count; i++) {
        long long population;
        long long rolling_deaths;
        long long numerator;
        if (!civs[i].alive) continue;
        population = civs[i].population;
        if (population <= 0) continue;
        rolling_deaths = plague_state_rolling_deaths_for_civ(absolute_month, i);
        numerator = infected_population[i] * 50 +
                    infected_population[i] * model->episode.severity * 3 +
                    rolling_deaths * 200;
        targets[i] = clamp_int((int)((numerator + population / 2) / population), 0, 100);
    }
}

int plague_disorder_target(int civ_id) {
    return civ_id >= 0 && civ_id < MAX_CIVS ? targets[civ_id] : 0;
}

int plague_disorder_step(int civ_id, int current, int *out_decay) {
    int target = plague_disorder_target(civ_id);
    int next = current;
    int decline = plague_state_get()->episode.active ? 6 : 10;
    if (current < target) next = current + clamp_int(target - current, 0, 12);
    else if (current > target) next = current - clamp_int(current - target, 0, decline);
    next = clamp_int(next, 0, 100);
    if (out_decay) *out_decay = next < current ? current - next : 0;
    return next;
}

void plague_disorder_max_current_target(int *out_current, int *out_target) {
    int current = 0;
    int target = 0;
    int i;
    for (i = 0; i < civ_count; i++) {
        if (!civs[i].alive) continue;
        if (civs[i].disorder_plague > current) current = civs[i].disorder_plague;
        if (targets[i] > target) target = targets[i];
    }
    if (out_current) *out_current = current;
    if (out_target) *out_target = target;
}
