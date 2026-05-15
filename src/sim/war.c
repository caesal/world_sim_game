#include "war.h"

#include "diplomacy.h"
#include "sim/disorder.h"
#include "sim/plague.h"
#include "sim/population.h"
#include "sim/simulation.h"
#include "sim/technology.h"
#include "sim/vassal.h"
#include "sim/war_front.h"
#include "sim/war_resolution.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_ACTIVE_WARS (MAX_CIVS * MAX_CIVS / 2)
#define WAR_MOBILIZATION_RATE 10
#define EXTREME_MOBILIZATION_RATE 18

static ActiveWar active_wars[MAX_ACTIVE_WARS];
static int support_casualties[MAX_CIVS];
static int total_started_wars = 0;

static int total_active_war_casualties(int civ_id);
static int support_share_for_front(int overlord, int vassal);
static int peace_desire(int civ_id, int casualties, int initial_soldiers, int initial_national);

static int is_valid_civ(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && civs[civ_id].alive;
}

static int mobilized_soldiers(int civ_id, int extreme) {
    int recruitable = population_recruitable_for_civ(civ_id);
    int rate = extreme ? EXTREME_MOBILIZATION_RATE : WAR_MOBILIZATION_RATE;

    return clamp(recruitable * rate / 100, 0, MAX_POPULATION);
}

static int current_national_soldiers(int civ_id) {
    return max(0, mobilized_soldiers(civ_id, 0) - total_active_war_casualties(civ_id));
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

static int active_war_count_for_principal(int civ_id) {
    int i;
    int count = 0;
    for (i = 0; i < MAX_ACTIVE_WARS; i++) {
        if (!active_wars[i].active) continue;
        if (active_wars[i].attacker == civ_id || active_wars[i].defender == civ_id) count++;
    }
    return count;
}

static int empty_war_slot(void) {
    int i;

    for (i = 0; i < MAX_ACTIVE_WARS; i++) {
        if (!active_wars[i].active) return i;
    }
    return -1;
}

static int *front_soldiers_ptr(ActiveWar *war, int civ_id) {
    if (!war || !war->active) return NULL;
    return war->attacker == civ_id ? &war->soldiers_a :
           war->defender == civ_id ? &war->soldiers_b : NULL;
}

static int front_count_for_civ(int civ_id) {
    int i;
    int count = 0;

    for (i = 0; i < MAX_ACTIVE_WARS; i++) {
        if (front_soldiers_ptr(&active_wars[i], civ_id)) count++;
    }
    return count;
}

static int deployed_soldiers_for_civ(int civ_id, ActiveWar *skip) {
    int i;
    int total = 0;
    for (i = 0; i < MAX_ACTIVE_WARS; i++) {
        int *soldiers;
        if (&active_wars[i] == skip) continue;
        soldiers = front_soldiers_ptr(&active_wars[i], civ_id);
        if (soldiers) total += *soldiers;
    }
    return total;
}

static int withdraw_from_fronts(int civ_id, ActiveWar *skip, int amount) {
    int total = deployed_soldiers_for_civ(civ_id, skip);
    int remaining = min(amount, total);
    int withdrawn = 0;
    int i;
    for (i = 0; i < MAX_ACTIVE_WARS && remaining > 0; i++) {
        int *soldiers;
        int take;
        if (&active_wars[i] == skip) continue;
        soldiers = front_soldiers_ptr(&active_wars[i], civ_id);
        if (!soldiers || *soldiers <= 0) continue;
        take = max(1, amount * *soldiers / max(1, total));
        take = clamp(take, 0, min(*soldiers, remaining));
        *soldiers -= take;
        remaining -= take;
        withdrawn += take;
    }
    return withdrawn;
}

static int allocate_front_share(ActiveWar *war, int civ_id) {
    int *front = front_soldiers_ptr(war, civ_id);
    int total = current_national_soldiers(civ_id);
    int fronts = max(1, front_count_for_civ(civ_id));
    int target = total / fronts;
    int deployed = deployed_soldiers_for_civ(civ_id, war);
    int reserve = max(0, total - deployed);
    int needed;
    if (!front || total <= 0) return 0;
    needed = max(0, target - reserve);
    if (needed > 0) reserve += withdraw_from_fronts(civ_id, war, needed);
    *front = min(target, reserve);
    return *front;
}

static void cap_deployed_to_national(int civ_id) {
    int national = current_national_soldiers(civ_id);
    int deployed = deployed_soldiers_for_civ(civ_id, NULL);
    if (deployed > national) withdraw_from_fronts(civ_id, NULL, deployed - national);
}

void war_reset(void) {
    memset(active_wars, 0, sizeof(active_wars));
    memset(support_casualties, 0, sizeof(support_casualties));
    total_started_wars = 0;
}

static int war_start_internal(int attacker, int defender, int allow_no_border) {
    int slot;
    int direct_target = defender;
    ActiveWar *war;

    if (!is_valid_civ(attacker) || !is_valid_civ(defender) || attacker == defender) return 0;
    if (vassal_overlord(attacker) >= 0) return 0;
    if (vassal_overlord(defender) >= 0) defender = vassal_overlord(defender);
    if (!is_valid_civ(defender) || attacker == defender) return 0;
    (void)allow_no_border;
    if (!war_has_active_front(attacker, defender) && !war_has_active_front(attacker, direct_target)) return 0;
    if (active_war_index(attacker, defender) >= 0) return 0;

    slot = empty_war_slot();
    if (slot < 0) return 0;

    war = &active_wars[slot];
    memset(war, 0, sizeof(*war));
    war->active = 1;
    war->attacker = attacker;
    war->defender = defender;
    war->initial_national_a = current_national_soldiers(attacker);
    war->initial_national_b = current_national_soldiers(defender);
    war->initial_soldiers_a = allocate_front_share(war, attacker);
    war->initial_soldiers_b = allocate_front_share(war, defender);
    total_started_wars++;
    event_log_push_structured(EVENT_TYPE_WAR_STARTED, EVENT_SEVERITY_WARNING,
                              attacker, defender, -1, -1, 0, 0, "");
    diplomacy_force_war(attacker, defender);
    return 1;
}

int war_start(int attacker, int defender) {
    return war_start_internal(attacker, defender, 0);
}

int war_start_independence(int attacker, int defender) {
    return war_start_internal(attacker, defender, 1);
}

int war_active_between(int civ_a, int civ_b) { return active_war_index(civ_a, civ_b) >= 0; }
ActiveWar war_state_between(int civ_a, int civ_b) {
    ActiveWar empty;
    int index = active_war_index(civ_a, civ_b);

    memset(&empty, 0, sizeof(empty));
    if (index < 0) return empty;
    empty = active_wars[index];
    empty.soldiers_a += vassal_total_callable_soldiers(empty.attacker) /
                        max(1, active_war_count_for_principal(empty.attacker));
    empty.soldiers_b += vassal_total_callable_soldiers(empty.defender) /
                        max(1, active_war_count_for_principal(empty.defender));
    return empty;
}

int war_estimated_soldiers(int civ_id) { return is_valid_civ(civ_id) ? mobilized_soldiers(civ_id, 0) : 0; }
int war_current_soldiers_for_civ(int civ_id) { return is_valid_civ(civ_id) ? current_national_soldiers(civ_id) : 0; }
int war_deployed_soldiers_for_civ(int civ_id) {
    return is_valid_civ(civ_id) ? deployed_soldiers_for_civ(civ_id, NULL) : 0;
}
int war_available_reserve_for_civ(int civ_id) {
    return is_valid_civ(civ_id) ? max(0, war_current_soldiers_for_civ(civ_id) -
                                      war_deployed_soldiers_for_civ(civ_id)) : 0;
}
int war_front_count_for_civ(int civ_id) { return is_valid_civ(civ_id) ? front_count_for_civ(civ_id) : 0; }
int war_active_for_civ(int civ_id) { return war_front_count_for_civ(civ_id) > 0; }
int war_vassal_support_used_for_overlord(int overlord, int vassal) {
    if (!is_valid_civ(overlord) || !is_valid_civ(vassal)) return 0;
    if (!vassal_is_direct(overlord, vassal)) return 0;
    return support_share_for_front(overlord, vassal);
}
int war_vassal_support_casualties(int vassal) {
    return is_valid_civ(vassal) ? support_casualties[vassal] : 0;
}
int war_peace_pressure_between(int civ_id, int other_id) {
    int index = active_war_index(civ_id, other_id);
    ActiveWar *war;
    if (index < 0) return 0;
    war = &active_wars[index];
    if (civ_id == war->attacker) {
        return peace_desire(civ_id, war->casualties_a, war->initial_soldiers_a, war->initial_national_a);
    }
    if (civ_id == war->defender) {
        return peace_desire(civ_id, war->casualties_b, war->initial_soldiers_b, war->initial_national_b);
    }
    return 0;
}
int war_total_started_count(void) { return total_started_wars; }

void war_end_direct_for_civ(int civ_id) {
    int i;
    for (i = 0; i < MAX_ACTIVE_WARS; i++) {
        if (!active_wars[i].active) continue;
        if (active_wars[i].attacker == civ_id || active_wars[i].defender == civ_id) {
            memset(&active_wars[i], 0, sizeof(active_wars[i]));
        }
    }
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

static void finish_war(ActiveWar *war, WarOutcome outcome, int margin) {
    war_apply_outcome(war->attacker, war->defender, outcome, margin);
    memset(war, 0, sizeof(*war));
}

static void end_war_severed_front(ActiveWar *war) {
    int attacker = war->attacker;
    int defender = war->defender;
    event_log_push_structured(EVENT_TYPE_WAR_FRONT_SEVERED, EVENT_SEVERITY_INFO,
                              attacker, defender, -1, -1, 0, 0, "");
    diplomacy_start_truce(attacker, defender, 5, 45);
    memset(war, 0, sizeof(*war));
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

static int support_share_for_front(int overlord, int vassal) {
    return vassal_callable_soldiers(vassal) / max(1, active_war_count_for_principal(overlord));
}

static int side_support_soldiers(int overlord) {
    int ids[MAX_CIVS];
    int count = vassal_collect_direct(overlord, ids, MAX_CIVS);
    int total = 0;
    int i;
    for (i = 0; i < count; i++) total += support_share_for_front(overlord, ids[i]);
    return total;
}

static int side_support_count(int overlord) {
    int ids[MAX_CIVS];
    int count = vassal_collect_direct(overlord, ids, MAX_CIVS);
    int used = 0;
    int i;
    for (i = 0; i < count; i++) {
        if (support_share_for_front(overlord, ids[i]) > 0) used++;
    }
    return used;
}

static int apply_support_casualties(int overlord) {
    int ids[MAX_CIVS];
    int count = vassal_collect_direct(overlord, ids, MAX_CIVS);
    int total = 0;
    int i;
    for (i = 0; i < count; i++) {
        int soldiers = support_share_for_front(overlord, ids[i]);
        int losses = soldiers * 15 / 1000;
        if (soldiers > 0 && losses <= 0) losses = 1;
        losses = min(losses, soldiers);
        support_casualties[ids[i]] += losses;
        apply_population_casualties(ids[i], losses);
        total += losses;
    }
    return total;
}

static int total_active_war_casualties(int civ_id) {
    int i;
    int total = 0;

    for (i = 0; i < MAX_ACTIVE_WARS; i++) {
        ActiveWar *war = &active_wars[i];
        if (!war->active) continue;
        if (war->attacker == civ_id) total += war->casualties_a;
        else if (war->defender == civ_id) total += war->casualties_b;
    }
    return total + support_casualties[civ_id];
}

static int peace_desire(int civ_id, int casualties, int initial_soldiers, int initial_national) {
    int desire;

    if (!is_valid_civ(civ_id)) return 100;
    desire = civs[civ_id].disorder + (initial_soldiers > 0 ? casualties * 70 / initial_soldiers : 0);
    desire += initial_national > 0 ? total_active_war_casualties(civ_id) * 45 / initial_national : 0;
    desire += max(0, front_count_for_civ(civ_id) - 1) * 8;
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
    if (!war_has_active_front(war->attacker, war->defender)) {
        end_war_severed_front(war);
        return;
    }

    cap_deployed_to_national(war->attacker);
    cap_deployed_to_national(war->defender);
    war->years++;
    if (war->soldiers_a <= 0) {
        finish_war(war, WAR_OUTCOME_DEFENDER_WIN, 3);
        return;
    }
    if (war->soldiers_b <= 0) {
        finish_war(war, WAR_OUTCOME_ATTACKER_WIN, 3);
        return;
    }
    if (war->initial_national_a > 0 && war_current_soldiers_for_civ(war->attacker) * 3 <= war->initial_national_a) {
        finish_war(war, WAR_OUTCOME_DEFENDER_WIN, 3);
        return;
    }
    if (war->initial_national_b > 0 && war_current_soldiers_for_civ(war->defender) * 3 <= war->initial_national_b) {
        finish_war(war, WAR_OUTCOME_ATTACKER_WIN, 3);
        return;
    }
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
    effective_a = scaled_soldiers_for_battle(war->attacker,
                                             war->soldiers_a + side_support_soldiers(war->attacker), 0);
    effective_b = scaled_soldiers_for_battle(war->defender,
                                             war->soldiers_b + side_support_soldiers(war->defender), 1);
    chance_a = effective_a * 1000 / max(1, effective_a + effective_b);
    chance_b = 1000 - chance_a;
    chance_a += technology_battle_chance_bonus(war->attacker) * 10;
    chance_b += technology_battle_chance_bonus(war->defender) * 10;
    chance_a = chance_a * 1000 / max(1, chance_a + chance_b);
    roll = rnd(1000);
    attacker_won = roll < chance_a;

    if (attacker_won) {
        casualties_a = casualty_per_mille(&war->soldiers_a,
                                          max(0, 55 - side_support_count(war->attacker) * 15));
        casualties_b = casualty_per_mille(&war->soldiers_b,
                                          max(0, 80 - side_support_count(war->defender) * 15));
        war->wins_a++;
    } else {
        casualties_a = casualty_per_mille(&war->soldiers_a,
                                          max(0, 80 - side_support_count(war->attacker) * 15));
        casualties_b = casualty_per_mille(&war->soldiers_b,
                                          max(0, 55 - side_support_count(war->defender) * 15));
        war->wins_b++;
    }

    war->casualties_a += casualties_a;
    war->casualties_b += casualties_b;
    apply_population_casualties(war->attacker, casualties_a);
    apply_population_casualties(war->defender, casualties_b);
    war->support_casualties_a += apply_support_casualties(war->attacker);
    war->support_casualties_b += apply_support_casualties(war->defender);
    cap_deployed_to_national(war->attacker);
    cap_deployed_to_national(war->defender);
    peace_a = peace_desire(war->attacker, war->casualties_a, war->initial_soldiers_a, war->initial_national_a);
    peace_b = peace_desire(war->defender, war->casualties_b, war->initial_soldiers_b, war->initial_national_b);
    if (!(peace_a >= 70 && peace_b >= 70)) return;
    loser = civs[war->attacker].disorder >= civs[war->defender].disorder ? war->attacker : war->defender;
    outcome = loser_outcome(loser, war);
    finish_war(war, outcome, max(1, war_owned_province_count(loser) / 5));
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
