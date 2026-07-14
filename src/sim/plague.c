#include "sim/plague.h"

#include "core/game_types.h"
#include "sim/plague_adjacency.h"
#include "sim/plague_disorder.h"
#include "sim/plague_diagnostics.h"
#include "sim/plague_immunity.h"
#include "sim/plague_metrics.h"
#include "sim/plague_rules.h"
#include "sim/plague_spread.h"
#include "sim/plague_state.h"
#include "sim/sea_lanes.h"

#include <limits.h>
#include <string.h>

static int absolute_month_now(void) {
    return plague_rules_absolute_month(year, month);
}

static int valid_city_id(int city_id) {
    return city_id >= 0 && city_id < city_count;
}

void plague_reset(void) {
    int i;
    plague_state_reset();
    plague_spread_reset();
    plague_adjacency_reset();
    plague_metrics_reset();
    plague_disorder_reset();
    plague_diagnostics_reset();
    for (i = 0; i < MAX_CIVS; i++) {
        civs[i].plague_random_immunity_months = 0;
        civs[i].plague_was_active_last_month = 0;
        civs[i].plague_recovery_months = 0;
    }
}

void plague_after_restore(void) {
    int i;
    plague_spread_reset();
    plague_adjacency_reset();
    plague_metrics_reset();
    plague_disorder_reset();
    plague_disorder_refresh_targets(absolute_month_now());
    plague_diagnostics_refresh(absolute_month_now());
    for (i = 0; i < MAX_CIVS; i++) {
        civs[i].plague_random_immunity_months = 0;
        civs[i].plague_was_active_last_month = 0;
        civs[i].plague_recovery_months = 0;
    }
}

int plague_update_month_step(PlagueUpdateState *state, int batch_size) {
    return plague_engine_update_month_step(state, batch_size);
}

void plague_update_month(void) {
    PlagueUpdateState state;
    memset(&state, 0, sizeof(state));
    while (!plague_update_month_step(&state, 16)) {}
}

int plague_city_active(int city_id) {
    const PlagueModelState *model = plague_state_get();
    return model->episode.active && valid_city_id(city_id) &&
           model->cities[city_id].active &&
           model->cities[city_id].infection_start_month <= absolute_month_now() &&
           model->cities[city_id].recovery_month > absolute_month_now();
}

int plague_city_severity(int city_id) {
    return plague_city_active(city_id) ? plague_state_get()->episode.severity : 0;
}

int plague_city_deaths_total(int city_id) {
    return valid_city_id(city_id) ? (int)plague_state_get()->cities[city_id].episode_deaths : 0;
}

int plague_city_months_left(int city_id) {
    int remaining;
    if (!plague_city_active(city_id)) return 0;
    remaining = plague_state_get()->cities[city_id].recovery_month - absolute_month_now();
    return remaining > 0 ? remaining : 0;
}

int plague_city_reinfection_cooldown_months(int city_id) {
    int remaining;
    const PlagueCityEpisodeState *city;
    if (!valid_city_id(city_id)) return 0;
    city = &plague_state_get()->cities[city_id];
    remaining = city->immunity_expiry_month - absolute_month_now();
    return remaining > 0 ? remaining : 0;
}

int plague_tile_severity(int x, int y) {
    int city_id;
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return 0;
    city_id = world[y][x].province_id;
    return plague_city_severity(city_id);
}

int plague_civ_active_count(int civ_id) {
    const PlagueModelState *model = plague_state_get();
    int count = 0;
    int i;
    if (civ_id < 0 || civ_id >= civ_count) return 0;
    for (i = 0; i < model->active_city_count; i++) {
        int city_id = model->active_city_ids[i];
        if (plague_city_active(city_id) && cities[city_id].owner == civ_id) count++;
    }
    return count;
}

int plague_active_for_civ(int civ_id) { return plague_civ_active_count(civ_id) > 0; }

int plague_civ_pressure(int civ_id) {
    if (!plague_active_for_civ(civ_id)) return 0;
    return plague_state_get()->episode.severity;
}

int plague_civ_deaths_total(int civ_id) {
    const PlagueModelState *model = plague_state_get();
    int64_t total = 0;
    int i;
    if (civ_id < 0 || civ_id >= civ_count) return 0;
    for (i = 0; i < model->ever_infected_city_count; i++) {
        int city_id = model->ever_infected_city_ids[i];
        if (valid_city_id(city_id) && cities[city_id].owner == civ_id) {
            total += model->cities[city_id].episode_deaths;
        }
    }
    return total > INT_MAX ? INT_MAX : (int)total;
}

int plague_civ_peak_severity(int civ_id) {
    return plague_active_for_civ(civ_id) ? plague_state_get()->episode.severity : 0;
}

int plague_civ_months_left(int civ_id) {
    const PlagueModelState *model = plague_state_get();
    int remaining = 0;
    int i;
    if (civ_id < 0 || civ_id >= civ_count) return 0;
    for (i = 0; i < model->active_city_count; i++) {
        int city_id = model->active_city_ids[i];
        if (plague_city_active(city_id) && cities[city_id].owner == civ_id) {
            int city_remaining = plague_city_months_left(city_id);
            if (city_remaining > remaining) remaining = city_remaining;
        }
    }
    return remaining;
}

int plague_random_immunity_months(int civ_id) {
    const PlagueModelState *model = plague_state_get();
    int maximum = 0;
    int city_id;
    if (civ_id < 0 || civ_id >= civ_count) return 0;
    for (city_id = 0; city_id < city_count; city_id++) {
        int remaining;
        if (cities[city_id].owner != civ_id) continue;
        remaining = model->cities[city_id].immunity_expiry_month - absolute_month_now();
        if (plague_immunity_effective_percent(&model->cities[city_id], absolute_month_now()) > 0 &&
            remaining > maximum) maximum = remaining;
    }
    return maximum;
}

int plague_random_immunity_civ_count(void) {
    int count = 0;
    int civ_id;
    for (civ_id = 0; civ_id < civ_count; civ_id++) {
        if (civs[civ_id].alive && plague_random_immunity_months(civ_id) > 0) count++;
    }
    return count;
}

int plague_global_active_state(int *first_city_id) {
    const PlagueModelState *model = plague_state_get();
    if (first_city_id) {
        *first_city_id = model->active_city_count > 0 ? model->active_city_ids[0] : -1;
    }
    return model->episode.active ? model->active_city_count : 0;
}

int plague_route_exposure(int route_id) {
    return sea_lanes_exposure(route_id);
}
