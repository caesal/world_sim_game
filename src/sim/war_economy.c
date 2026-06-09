#include "sim/war_economy.h"

#include "core/game_state.h"
#include "sim/economy.h"

static void try_hire_for_side(ActiveWar *war, int attacker_side) {
    int civ_id;
    int enemy_id;
    int own;
    int enemy;
    int cost = 0;
    int hired;
    int *temp;
    int *used;
    if (!war || !war->active) return;
    civ_id = attacker_side ? war->attacker : war->defender;
    enemy_id = attacker_side ? war->defender : war->attacker;
    temp = attacker_side ? &war->temporary_soldiers_a : &war->temporary_soldiers_b;
    used = attacker_side ? &war->mercenary_hired_a : &war->mercenary_hired_b;
    if (*used || civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return;
    if (civs[civ_id].mercenary_cooldown_months > 0) return;
    own = (attacker_side ? war->soldiers_a : war->soldiers_b) + *temp;
    enemy = (attacker_side ? war->soldiers_b : war->soldiers_a) +
            (attacker_side ? war->temporary_soldiers_b : war->temporary_soldiers_a);
    if (enemy <= 0 || own * 100 >= enemy * 80) return;
    hired = economy_mercenary_hire_capacity(civ_id, own, enemy, &cost);
    if (hired <= 0 || cost <= 0 || !economy_can_spend_after_floor(civ_id, cost)) return;
    if (economy_spend_treasury(civ_id, cost) != cost) return;
    *temp += hired;
    *used = 1;
    economy_start_mercenary_cooldown(civ_id);
    event_log_push_structured(EVENT_TYPE_MERCENARIES_HIRED, EVENT_SEVERITY_WARNING,
                              civ_id, enemy_id, -1, -1, hired, cost, "");
}

void war_economy_try_hire_mercenaries(ActiveWar *war) {
    try_hire_for_side(war, 1);
    try_hire_for_side(war, 0);
}

int war_economy_absorb_casualties(ActiveWar *war, int attacker_side, int casualties) {
    int *temp;
    int absorbed;
    if (!war || casualties <= 0) return max(0, casualties);
    temp = attacker_side ? &war->temporary_soldiers_a : &war->temporary_soldiers_b;
    absorbed = min(max(0, *temp), casualties);
    *temp -= absorbed;
    return casualties - absorbed;
}

int war_economy_temporary_soldiers_for_civ(const ActiveWar *war, int civ_id) {
    if (!war || !war->active) return 0;
    if (war->attacker == civ_id) return max(0, war->temporary_soldiers_a);
    if (war->defender == civ_id) return max(0, war->temporary_soldiers_b);
    return 0;
}
