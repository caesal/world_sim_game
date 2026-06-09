#include "sim/economy.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "sim/decision_snapshot.h"
#include "sim/disorder.h"
#include "sim/population.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/technology.h"

#define STABILITY_OFFSET 15
#define STABILITY_PROJECT_MONTHS 60
#define STABILITY_COOLDOWN_MONTHS 60
#define MERCENARY_COOLDOWN_MONTHS 60

static int population_units_for_civ(int civ_id) {
    PopulationSummary pop = population_country_summary(civ_id);
    return max(10, pop.total / 1000);
}

int economy_settlement_cycle_years(int civ_id) {
    int stage = civ_id >= 0 && civ_id < civ_count ? civs[civ_id].tech_stage : 0;
    if (stage >= 8) return 4;
    if (stage >= 4) return 6;
    return 10;
}

static int treasury_cap_for_civ(int civ_id) {
    PopulationSummary pop = population_country_summary(civ_id);
    CountrySummary country = summarize_country(civ_id);
    Civilization *civ = civ_id >= 0 && civ_id < civ_count ? &civs[civ_id] : NULL;
    int population_units;
    int reserve_years_x12;

    if (!civ || !civ->alive) return 0;
    population_units = max(10, pop.total / 1000);
    reserve_years_x12 = 24 + civ->governance * 3 + civ->commerce * 2 +
                        civ->tech_stage * 3 + min(country.cities, 12) * 2 +
                        min(country.ports, 6) * 3;
    return max(0, population_units * reserve_years_x12 * 10 / 12);
}

static void economy_normalize_civ_internal(int civ_id, int clamp_treasury_to_cap) {
    Civilization *civ;
    if (civ_id < 0 || civ_id >= civ_count) return;
    civ = &civs[civ_id];
    civ->treasury_cap = max(0, treasury_cap_for_civ(civ_id));
    civ->treasury = clamp_treasury_to_cap ? clamp(civ->treasury, 0, civ->treasury_cap) :
                    max(0, civ->treasury);
    civ->treasury_pending_surplus = max(0, civ->treasury_pending_surplus);
    civ->treasury_last_deficit = max(0, civ->treasury_last_deficit);
    civ->resource_pressure = clamp(civ->resource_pressure, 0, 100);
    civ->treasury_deficit_years = clamp(civ->treasury_deficit_years, 0, 100);
    civ->treasury_stability_months_left = max(0, civ->treasury_stability_months_left);
    civ->treasury_stability_cooldown_months = max(0, civ->treasury_stability_cooldown_months);
    civ->mercenary_cooldown_months = max(0, civ->mercenary_cooldown_months);
}

void economy_normalize_civ(int civ_id) {
    economy_normalize_civ_internal(civ_id, 1);
}

void economy_initialize_civ(int civ_id) {
    Civilization *civ;
    if (civ_id < 0 || civ_id >= civ_count) return;
    civ = &civs[civ_id];
    civ->treasury_pending_surplus = 0;
    civ->treasury_last_annual_balance = 0;
    civ->treasury_last_deficit = 0;
    civ->resource_pressure = 0;
    civ->treasury_deficit_years = 0;
    civ->treasury_stability_months_left = 0;
    civ->treasury_stability_cooldown_months = 0;
    civ->mercenary_cooldown_months = 0;
    economy_normalize_civ(civ_id);
    civ->treasury = civ->treasury_cap / 4;
}

int economy_effective_disorder_for_civ(int civ_id) {
    Civilization *civ;
    int value;
    if (civ_id < 0 || civ_id >= civ_count) return 0;
    civ = &civs[civ_id];
    value = civ->disorder;
    if (civ->treasury_stability_months_left > 0) value -= STABILITY_OFFSET;
    return clamp(value, 0, 100);
}

int economy_treasury_reserve_floor(int civ_id) {
    if (civ_id < 0 || civ_id >= civ_count) return 0;
    return max(0, civs[civ_id].treasury_cap * 10 / 100);
}

int economy_stability_project_cost(int civ_id) {
    int base;
    int severity_extra;
    int effective;
    if (civ_id < 0 || civ_id >= civ_count) return 0;
    effective = economy_effective_disorder_for_civ(civ_id);
    base = max(civs[civ_id].treasury_cap * 8 / 100, population_units_for_civ(civ_id) * 8);
    severity_extra = clamp(effective - 65, 0, 35);
    return max(1, base * (100 + severity_extra) / 100);
}

int economy_spend_treasury(int civ_id, int amount) {
    Civilization *civ;
    int spent;
    if (civ_id < 0 || civ_id >= civ_count || amount <= 0) return 0;
    civ = &civs[civ_id];
    spent = min(max(0, civ->treasury), amount);
    civ->treasury -= spent;
    if (spent > 0) dirty_mark_civ_stats();
    return spent;
}

int economy_can_spend_after_floor(int civ_id, int amount) {
    if (civ_id < 0 || civ_id >= civ_count || amount < 0) return 0;
    return civs[civ_id].treasury - amount >= economy_treasury_reserve_floor(civ_id);
}

void economy_start_mercenary_cooldown(int civ_id) {
    if (civ_id < 0 || civ_id >= civ_count) return;
    civs[civ_id].mercenary_cooldown_months = MERCENARY_COOLDOWN_MONTHS;
}

int economy_region_asset(int region_id) {
    const NaturalRegion *region = regions_get(region_id);
    const TerrainStats *stats;
    if (!region || !region->alive) return 0;
    stats = &region->total_stats;
    return max(0, stats->food + stats->livestock + stats->water +
                  stats->pop_capacity + stats->money + stats->habitability);
}

int economy_owned_region_asset_total(int civ_id) {
    int total = 0;
    int i;
    if (civ_id < 0 || civ_id >= civ_count) return 0;
    for (i = 0; i < region_count; i++) {
        if (natural_regions[i].alive && natural_regions[i].owner_civ == civ_id) {
            total += economy_region_asset(i);
        }
    }
    return total;
}

int economy_region_list_asset(const int *regions, int count) {
    int total = 0;
    int i;
    if (!regions || count <= 0) return 0;
    for (i = 0; i < count; i++) total += economy_region_asset(regions[i]);
    return total;
}

int economy_split_treasury_snapshot_to_child(int parent, int child, int parent_treasury_snapshot,
                                             int child_asset, int parent_total_asset) {
    long long raw;
    int room;
    int transfer;
    if (parent < 0 || parent >= civ_count || child < 0 || child >= civ_count) return 0;
    if (!civs[parent].alive || !civs[child].alive || child_asset <= 0 || parent_total_asset <= 0) return 0;
    raw = (long long)max(0, parent_treasury_snapshot) * child_asset / parent_total_asset;
    economy_normalize_civ(child);
    room = max(0, civs[child].treasury_cap - civs[child].treasury);
    transfer = min((int)min(raw, (long long)max(0, civs[parent].treasury)), room);
    if (transfer <= 0) {
        economy_normalize_civ_internal(parent, 0);
        return 0;
    }
    civs[parent].treasury -= transfer;
    civs[child].treasury += transfer;
    economy_normalize_civ(child);
    economy_normalize_civ_internal(parent, 0);
    dirty_mark_civ_stats();
    return transfer;
}

int economy_split_treasury_to_child(int parent, int child, int child_asset, int parent_total_asset) {
    if (parent < 0 || parent >= civ_count) return 0;
    return economy_split_treasury_snapshot_to_child(parent, child, max(0, civs[parent].treasury),
                                                   child_asset, parent_total_asset);
}

static int annual_surplus_for_civ(int civ_id, int balance_people) {
    CountrySummary country = summarize_country(civ_id);
    Civilization *civ = &civs[civ_id];
    int surplus_units = max(0, balance_people / 1000);
    int money_quality = 70 + country.money * 3;
    int commerce_multiplier = 100 + civ->commerce * 2;
    int governance_multiplier = 90 + civ->governance * 2;
    int tech_multiplier = technology_resource_percent(civ_id);
    int stability_multiplier = disorder_productivity_percent(economy_effective_disorder_for_civ(civ_id));
    long long value = surplus_units;

    value = value * money_quality / 100;
    value = value * commerce_multiplier / 100;
    value = value * governance_multiplier / 100;
    value = value * tech_multiplier / 100;
    value = value * stability_multiplier / 100;
    return clamp((int)value, 0, 1000000000);
}

static void update_resource_pressure(Civilization *civ, int support_capacity,
                                     int remaining_deficit_people, int fully_covered) {
    int base_pressure = 0;
    int bankruptcy_bonus = 0;
    int prolonged_bonus;
    if (remaining_deficit_people > 0) {
        base_pressure = remaining_deficit_people * 100 / max(1, support_capacity);
        if (civ->treasury <= 0) bankruptcy_bonus = 12;
        civ->treasury_deficit_years = clamp(civ->treasury_deficit_years + 1, 0, 100);
    } else if (fully_covered || civ->treasury > 0) {
        civ->treasury_deficit_years = max(0, civ->treasury_deficit_years - 1);
    }
    prolonged_bonus = min(20, civ->treasury_deficit_years * 2);
    civ->resource_pressure = clamp(base_pressure + bankruptcy_bonus + prolonged_bonus, 0, 100);
}

static void settle_surplus_if_due(int civ_id) {
    Civilization *civ = &civs[civ_id];
    int cycle = economy_settlement_cycle_years(civ_id);
    if (cycle <= 0 || year % cycle != 0 || civ->treasury_pending_surplus <= 0) return;
    civ->treasury = clamp(civ->treasury + civ->treasury_pending_surplus, 0, civ->treasury_cap);
    civ->treasury_pending_surplus = 0;
}

static void economy_update_year_one(int civ_id) {
    Civilization *civ;
    PopulationSummary pop;
    int balance_people;
    int fully_covered = 0;

    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return;
    civ = &civs[civ_id];
    economy_normalize_civ(civ_id);
    pop = population_country_summary(civ_id);
    balance_people = pop.carrying_capacity - pop.total;
    civ->treasury_last_annual_balance = 0;
    civ->treasury_last_deficit = 0;
    if (balance_people > 0) {
        int surplus = annual_surplus_for_civ(civ_id, balance_people);
        civ->treasury_pending_surplus = clamp(civ->treasury_pending_surplus + surplus, 0, civ->treasury_cap);
        civ->treasury_last_annual_balance = surplus;
        update_resource_pressure(civ, pop.carrying_capacity, 0, 1);
        settle_surplus_if_due(civ_id);
    } else if (balance_people < 0) {
        int deficit_people = -balance_people;
        int deficit_units = (deficit_people + 999) / 1000;
        int spent = economy_spend_treasury(civ_id, deficit_units);
        int remaining = max(0, deficit_people - spent * 1000);
        civ->treasury_last_deficit = deficit_units;
        civ->treasury_last_annual_balance = -deficit_units;
        fully_covered = remaining <= 0;
        update_resource_pressure(civ, pop.carrying_capacity, remaining, fully_covered);
    } else {
        update_resource_pressure(civ, pop.carrying_capacity, 0, 1);
    }
    economy_normalize_civ(civ_id);
}

static int stability_is_top_intent(int civ_id) {
    DecisionSnapshot snapshot;
    decision_snapshot_for_civ(civ_id, &snapshot);
    return snapshot.stability_weight >= snapshot.expansion_weight &&
           snapshot.stability_weight >= snapshot.war_weight;
}

static void try_start_stability_project(int civ_id) {
    Civilization *civ = &civs[civ_id];
    int cost;
    if (civ->treasury_stability_months_left > 0 || civ->treasury_stability_cooldown_months > 0) return;
    if (economy_effective_disorder_for_civ(civ_id) < 65) return;
    if (!stability_is_top_intent(civ_id)) return;
    cost = economy_stability_project_cost(civ_id);
    if (!economy_can_spend_after_floor(civ_id, cost)) return;
    economy_spend_treasury(civ_id, cost);
    civ->treasury_stability_months_left = STABILITY_PROJECT_MONTHS;
    event_log_push_structured(EVENT_TYPE_STABILITY_PROJECT, EVENT_SEVERITY_INFO,
                              civ_id, -1, -1, -1, cost, STABILITY_OFFSET, "");
}

void economy_update_month_all(void) {
    int i;
    int changed = 0;
    for (i = 0; i < civ_count; i++) {
        Civilization *civ = &civs[i];
        if (!civ->alive) continue;
        if (civ->treasury_stability_months_left > 0 &&
            --civ->treasury_stability_months_left == 0) {
            civ->treasury_stability_cooldown_months = STABILITY_COOLDOWN_MONTHS;
            changed = 1;
        } else if (civ->treasury_stability_cooldown_months > 0) {
            civ->treasury_stability_cooldown_months--;
            changed = 1;
        }
        if (civ->mercenary_cooldown_months > 0) {
            civ->mercenary_cooldown_months--;
            changed = 1;
        }
        try_start_stability_project(i);
    }
    if (changed) dirty_mark_civ_stats();
}

int economy_update_year_step(int *cursor, int max_civs) {
    int processed = 0;
    if (!cursor) return 1;
    if (max_civs <= 0) max_civs = 1;
    while (*cursor < civ_count && processed < max_civs) {
        economy_update_year_one(*cursor);
        (*cursor)++;
        processed++;
    }
    if (*cursor >= civ_count) {
        dirty_mark_civ_stats();
        return 1;
    }
    return 0;
}

void economy_update_year_all(void) {
    int cursor = 0;
    while (!economy_update_year_step(&cursor, 8)) {}
}

int economy_indemnity_cost_per_province(int loser, int winner) {
    int base;
    (void)winner;
    if (loser < 0 || loser >= civ_count) return 1;
    base = max(1, civs[loser].treasury_cap * 15 / 100);
    return max(base, population_units_for_civ(loser) * 4);
}

int economy_mercenary_hire_capacity(int civ_id, int own_soldiers, int enemy_soldiers, int *out_cost) {
    int spendable;
    int army_gap;
    int max_hire_by_gap;
    int max_hire_by_army;
    int max_hire_by_treasury;
    int temporary_soldiers;
    if (out_cost) *out_cost = 0;
    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return 0;
    if (civs[civ_id].mercenary_cooldown_months > 0) return 0;
    if (own_soldiers * 100 >= enemy_soldiers * 80) return 0;
    spendable = max(0, civs[civ_id].treasury - economy_treasury_reserve_floor(civ_id));
    if (spendable <= 0) return 0;
    army_gap = max(0, enemy_soldiers - own_soldiers);
    max_hire_by_gap = army_gap / 2;
    max_hire_by_army = max(1, max(1, own_soldiers) * 25 / 100);
    max_hire_by_treasury = spendable * 50;
    temporary_soldiers = min(max_hire_by_gap, min(max_hire_by_army, max_hire_by_treasury));
    if (temporary_soldiers <= 0) return 0;
    if (out_cost) *out_cost = (temporary_soldiers + 49) / 50;
    return temporary_soldiers;
}
