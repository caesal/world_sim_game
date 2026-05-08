#include "war.h"

#include "diplomacy.h"
#include "sim/disorder.h"
#include "sim/plague.h"
#include "sim/population.h"
#include "sim/simulation.h"
#include "sim/technology.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_ACTIVE_WARS (MAX_CIVS * MAX_CIVS / 2)
/* War upkeep/economy hooks are intentionally deferred; this module models mobilization and casualties. */
#define WAR_MOBILIZATION_RATE 10
#define EXTREME_MOBILIZATION_RATE 18

static ActiveWar active_wars[MAX_ACTIVE_WARS];
static int total_started_wars = 0;

static int owned_province_count(int civ_id);

static int is_valid_civ(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && civs[civ_id].alive;
}

static int resource_deficit_value(int value, int target) {
    return clamp(target - value, 0, target);
}

static int mobilized_soldiers(int civ_id, int extreme) {
    int recruitable = population_recruitable_for_civ(civ_id);
    int rate = extreme ? EXTREME_MOBILIZATION_RATE : WAR_MOBILIZATION_RATE;

    return clamp(recruitable * rate / 100, 0, MAX_POPULATION);
}

static int has_border_province_contact(int civ_a, int civ_b) {
    static const int dirs[4][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    };
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int d;

            if (world[y][x].owner != civ_a || world[y][x].province_id < 0) continue;
            for (d = 0; d < 4; d++) {
                int nx = x + dirs[d][0];
                int ny = y + dirs[d][1];

                if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
                if (world[ny][nx].owner == civ_b && world[ny][nx].province_id >= 0) return 1;
            }
        }
    }
    return 0;
}

static int active_war_index(int civ_a, int civ_b) {
    int i;

    for (i = 0; i < MAX_ACTIVE_WARS; i++) {
        ActiveWar *war = &active_wars[i];
        if (!war->active) continue;
        if ((war->attacker == civ_a && war->defender == civ_b) ||
            (war->attacker == civ_b && war->defender == civ_a)) {
            return i;
        }
    }
    return -1;
}

static int empty_war_slot(void) {
    int i;

    for (i = 0; i < MAX_ACTIVE_WARS; i++) {
        if (!active_wars[i].active) return i;
    }
    return -1;
}

void war_reset(void) {
    memset(active_wars, 0, sizeof(active_wars));
    total_started_wars = 0;
}

int war_start(int attacker, int defender) {
    int slot;
    ActiveWar *war;

    if (!is_valid_civ(attacker) || !is_valid_civ(defender) || attacker == defender) return 0;
    if (!has_border_province_contact(attacker, defender)) return 0;
    if (active_war_index(attacker, defender) >= 0) return 0;

    slot = empty_war_slot();
    if (slot < 0) return 0;

    war = &active_wars[slot];
    memset(war, 0, sizeof(*war));
    war->active = 1;
    war->attacker = attacker;
    war->defender = defender;
    war->initial_soldiers_a = mobilized_soldiers(attacker, 0);
    war->initial_soldiers_b = mobilized_soldiers(defender, 0);
    war->soldiers_a = war->initial_soldiers_a;
    war->soldiers_b = war->initial_soldiers_b;
    total_started_wars++;
    {
        char text[EVENT_LOG_LEN];
        snprintf(text, sizeof(text), "[War] War started: %s vs %s.",
                 civilization_display_name_for_language(attacker, 0),
                 civilization_display_name_for_language(defender, 0));
        event_log_push(text);
    }
    diplomacy_force_war(attacker, defender);
    return 1;
}

int war_active_between(int civ_a, int civ_b) {
    return active_war_index(civ_a, civ_b) >= 0;
}

ActiveWar war_state_between(int civ_a, int civ_b) {
    ActiveWar empty;
    int index = active_war_index(civ_a, civ_b);

    memset(&empty, 0, sizeof(empty));
    if (index < 0) return empty;
    return active_wars[index];
}

int war_estimated_soldiers(int civ_id) {
    if (!is_valid_civ(civ_id)) return 0;
    return mobilized_soldiers(civ_id, 0);
}

int war_current_soldiers_for_civ(int civ_id) {
    int i;
    int total = 0;
    int at_war = 0;

    if (!is_valid_civ(civ_id)) return 0;
    for (i = 0; i < MAX_ACTIVE_WARS; i++) {
        ActiveWar *war = &active_wars[i];

        if (!war->active) continue;
        if (war->attacker == civ_id) {
            total += war->soldiers_a;
            at_war = 1;
        } else if (war->defender == civ_id) {
            total += war->soldiers_b;
            at_war = 1;
        }
    }
    return at_war ? total : war_estimated_soldiers(civ_id);
}

int war_total_started_count(void) {
    return total_started_wars;
}

static void apply_population_casualties(int civ_id, int soldier_casualties) {
    int population_losses;

    if (soldier_casualties <= 0) return;
    population_losses = population_apply_casualties(civ_id, soldier_casualties);
    if (population_losses > 0) {
        disorder_add_war_deaths(civ_id, population_losses);
        plague_notify_war_casualties(civ_id, population_losses);
    }
}

static void update_supply_state(ActiveWar *war) {
    CountrySummary a = summarize_country(war->attacker);
    CountrySummary b = summarize_country(war->defender);

    if (a.food < 3 || a.money < 3 || a.water < 2) war->supply_fail_a++;
    else war->supply_fail_a = 0;
    if (b.food < 3 || b.money < 3 || b.water < 2) war->supply_fail_b++;
    else war->supply_fail_b = 0;
}

static int province_borders_owner(int province_id, int owner) {
    static const int dirs[4][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    };
    int x;
    int y;

    if (province_id < 0 || owner < 0) return 0;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int d;

            if (world[y][x].province_id != province_id) continue;
            for (d = 0; d < 4; d++) {
                int nx = x + dirs[d][0];
                int ny = y + dirs[d][1];

                if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
                if (world[ny][nx].owner == owner && world[ny][nx].province_id >= 0) return 1;
            }
        }
    }
    return 0;
}

static int province_value_for_winner(int province_id, int winner) {
    RegionSummary province = summarize_city_region(province_id);
    CountrySummary country = summarize_country(winner);
    int score = province.tiles / 2 + province.population / 12 + province.habitability * 4;

    score += province.food * resource_deficit_value(country.food, 5) * 2;
    score += province.water * resource_deficit_value(country.water, 5) * 3;
    score += province.minerals * resource_deficit_value(country.minerals, 5) * 2;
    score += province.wood * resource_deficit_value(country.wood, 5) * 2;
    score += province.stone * resource_deficit_value(country.stone, 5) * 2;
    score += province.money * 3;
    if (cities[province_id].port) score += 18;
    return score + rnd(16);
}

static int pick_border_province(int loser, int winner, int used[MAX_CITIES]) {
    int i;
    int best = -1;
    int best_score = -1000000;

    for (i = 0; i < city_count; i++) {
        int score;

        if (!cities[i].alive || cities[i].owner != loser || used[i]) continue;
        if (!province_borders_owner(i, winner)) continue;
        score = province_value_for_winner(i, winner);
        if (score > best_score) {
            best_score = score;
            best = i;
        }
    }
    return best;
}

static int any_city_owned_by(int owner) {
    int i;

    for (i = 0; i < city_count; i++) {
        if (cities[i].alive && cities[i].owner == owner) return 1;
    }
    return 0;
}

static int capital_candidate_score(int city_id, int owner, int enemy) {
    RegionSummary province = summarize_city_region(city_id);
    int score;

    if (!cities[city_id].alive || cities[city_id].owner != owner) return -1000000;
    score = cities[city_id].population / 2 + province.food * 4 + province.water * 4 +
            province.money * 3 + province.habitability * 4 + province.tiles;
    if (cities[city_id].port) score += 20;
    if (province_borders_owner(city_id, enemy)) score -= 60;
    return score;
}

static void choose_new_capital(int loser, int winner) {
    int i;
    int best = -1;
    int best_score = -1000000;

    for (i = 0; i < city_count; i++) {
        int score;

        if (cities[i].alive && cities[i].owner == loser) cities[i].capital = 0;
        score = capital_candidate_score(i, loser, winner);
        if (score > best_score) {
            best_score = score;
            best = i;
        }
    }

    if (best >= 0) {
        cities[best].capital = 1;
        civs[loser].capital_city = best;
    } else {
        civs[loser].capital_city = -1;
        civs[loser].alive = 0;
    }
}

static void handle_capital_loss(int loser, int winner) {
    int extra_used[MAX_CITIES] = {0};

    civs[loser].disorder_stability = clamp(civs[loser].disorder_stability + 18, 0, 100);
    civs[loser].disorder = clamp(civs[loser].disorder + 18, 0, 100);
    civs[loser].cohesion = clamp(civs[loser].cohesion - 1, 0, 10);

    if (!any_city_owned_by(loser)) {
        civs[loser].alive = 0;
        civs[loser].capital_city = -1;
        return;
    }

    choose_new_capital(loser, winner);
    if (civs[loser].disorder >= 70) {
        int province_id = pick_border_province(loser, winner, extra_used);
        if (province_id >= 0) {
            extra_used[province_id] = 1;
            cities[province_id].capital = 0;
            world_claim_city_region(province_id, winner);
            civs[loser].disorder = clamp(civs[loser].disorder + 8, 0, 100);
        }
        choose_new_capital(loser, winner);
    }
}

static int transfer_border_provinces(int loser, int winner, int count) {
    int used[MAX_CITIES] = {0};
    int transferred = 0;

    while (transferred < count) {
        int province_id = pick_border_province(loser, winner, used);
        int capital_lost;

        if (province_id < 0) break;
        used[province_id] = 1;
        capital_lost = province_id == civs[loser].capital_city || cities[province_id].capital;
        cities[province_id].capital = 0;
        if (capital_lost) civs[loser].capital_city = -1;
        world_claim_city_region(province_id, winner);
        civs[loser].disorder = clamp(civs[loser].disorder + 8, 0, 100);
        civs[loser].disorder_stability = clamp(civs[loser].disorder_stability + 8, 0, 100);
        transferred++;
        if (capital_lost) handle_capital_loss(loser, winner);
    }
    return transferred;
}

static void finish_war(ActiveWar *war, WarOutcome outcome, int margin) {
    int winner = -1;
    int loser = -1;
    int province_count = 0;

    if (outcome == WAR_OUTCOME_ATTACKER_WIN) {
        winner = war->attacker;
        loser = war->defender;
    } else if (outcome == WAR_OUTCOME_DEFENDER_WIN) {
        winner = war->defender;
        loser = war->attacker;
    }

    if (winner >= 0 && loser >= 0 && is_valid_civ(winner) && is_valid_civ(loser)) {
        int transferred;
        (void)margin;
        province_count = max(1, owned_province_count(loser) / 5);
        transferred = transfer_border_provinces(loser, winner, province_count);
        if (transferred == 0) {
            civs[loser].disorder = clamp(civs[loser].disorder + 10, 0, 100);
        }
        if (transferred == 0 || (civs[loser].disorder >= 80 && civs[loser].cohesion <= 3)) {
            diplomacy_start_vassal(winner, loser, margin >= 3 ? 18 : 25);
        } else {
            diplomacy_start_truce(winner, loser, 10, margin >= 3 ? 20 : 30);
        }
    } else if (is_valid_civ(war->attacker) && is_valid_civ(war->defender)) {
        diplomacy_start_truce(war->attacker, war->defender, 6, 45);
    }

    world_recalculate_territory();
    world_invalidate_region_cache();
    memset(war, 0, sizeof(*war));
}

static int owned_province_count(int civ_id) {
    int i;
    int count = 0;

    for (i = 0; i < city_count; i++) {
        if (cities[i].alive && cities[i].owner == civ_id) count++;
    }
    return count;
}

static int scaled_soldiers_for_battle(int civ_id, int soldiers, int defending) {
    if (defending) soldiers = soldiers * technology_defense_army_percent(civ_id) / 100;
    return max(1, soldiers);
}

static int casualty_per_mille(int *soldiers, int per_mille) {
    int casualties;

    if (*soldiers <= 0 || per_mille <= 0) return 0;
    casualties = *soldiers * per_mille / 1000;
    if (casualties <= 0) casualties = 1;
    if (casualties > *soldiers) casualties = *soldiers;
    *soldiers -= casualties;
    return casualties;
}

static int peace_desire(int civ_id, int casualties, int initial_soldiers) {
    int desire;

    if (!is_valid_civ(civ_id)) return 100;
    desire = civs[civ_id].disorder + (initial_soldiers > 0 ? casualties * 70 / initial_soldiers : 0);
    desire += population_pressure_for_civ(civ_id) > 110 ? 12 : 0;
    return clamp(desire, 0, 100);
}

static WarOutcome loser_outcome(int loser, ActiveWar *war) {
    if (loser == war->attacker) return WAR_OUTCOME_DEFENDER_WIN;
    if (loser == war->defender) return WAR_OUTCOME_ATTACKER_WIN;
    return WAR_OUTCOME_STALEMATE;
}

static void run_war_year(ActiveWar *war) {
    int effective_a;
    int effective_b;
    int chance_a;
    int chance_b;
    int roll;
    int attacker_won;
    int casualties_a;
    int casualties_b;
    int peace_a;
    int peace_b;
    int loser = -1;
    WarOutcome outcome;

    if (!is_valid_civ(war->attacker) || !is_valid_civ(war->defender)) {
        memset(war, 0, sizeof(*war));
        return;
    }

    war->years++;
    if (war->initial_soldiers_a > 0 && war->soldiers_a * 3 <= war->initial_soldiers_a) {
        finish_war(war, WAR_OUTCOME_DEFENDER_WIN, 3);
        return;
    }
    if (war->initial_soldiers_b > 0 && war->soldiers_b * 3 <= war->initial_soldiers_b) {
        finish_war(war, WAR_OUTCOME_ATTACKER_WIN, 3);
        return;
    }
    if (war->years % 3 != 0) return;

    update_supply_state(war);
    effective_a = scaled_soldiers_for_battle(war->attacker, war->soldiers_a, 0);
    effective_b = scaled_soldiers_for_battle(war->defender, war->soldiers_b, 1);
    chance_a = effective_a * 1000 / max(1, effective_a + effective_b);
    chance_b = 1000 - chance_a;
    chance_a += technology_battle_chance_bonus(war->attacker) * 10;
    chance_b += technology_battle_chance_bonus(war->defender) * 10;
    chance_a = chance_a * 1000 / max(1, chance_a + chance_b);
    roll = rnd(1000);
    attacker_won = roll < chance_a;

    if (attacker_won) {
        casualties_a = casualty_per_mille(&war->soldiers_a, 55);
        casualties_b = casualty_per_mille(&war->soldiers_b, 80);
        war->wins_a++;
    } else {
        casualties_a = casualty_per_mille(&war->soldiers_a, 80);
        casualties_b = casualty_per_mille(&war->soldiers_b, 55);
        war->wins_b++;
    }

    war->casualties_a += casualties_a;
    war->casualties_b += casualties_b;
    apply_population_casualties(war->attacker, casualties_a);
    apply_population_casualties(war->defender, casualties_b);
    peace_a = peace_desire(war->attacker, war->casualties_a, war->initial_soldiers_a);
    peace_b = peace_desire(war->defender, war->casualties_b, war->initial_soldiers_b);
    if (!(peace_a >= 70 && peace_b >= 70)) return;
    loser = civs[war->attacker].disorder >= civs[war->defender].disorder ? war->attacker : war->defender;
    outcome = loser_outcome(loser, war);
    finish_war(war, outcome, max(1, owned_province_count(loser) / 5));
}

void war_update_year(void) {
    int i;

    for (i = 0; i < MAX_ACTIVE_WARS; i++) {
        if (active_wars[i].active) run_war_year(&active_wars[i]);
    }
}

const char *war_outcome_name(WarOutcome outcome) {
    switch (outcome) {
        case WAR_OUTCOME_NONE: return "None";
        case WAR_OUTCOME_ATTACKER_WIN: return "Attacker Win";
        case WAR_OUTCOME_DEFENDER_WIN: return "Defender Win";
        case WAR_OUTCOME_STALEMATE: return "Stalemate";
        default: return "Unknown";
    }
}
