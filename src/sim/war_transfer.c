#include "sim/war.h"

#include "sim/war_internal.h"
#include "sim/war_terminal.h"

void war_transfer_civ_identity(int from_civ, int to_civ) {
    int i;
    if (from_civ < 0 || from_civ >= MAX_CIVS || to_civ < 0 || to_civ >= MAX_CIVS ||
        from_civ == to_civ) return;
    for (i = 0; i < MAX_ACTIVE_WARS; i++) {
        int attacker;
        int defender;
        if (!active_wars[i].active) continue;
        attacker = active_wars[i].attacker == from_civ ? to_civ : active_wars[i].attacker;
        defender = active_wars[i].defender == from_civ ? to_civ : active_wars[i].defender;
        if (attacker == defender) {
            war_terminal_interrupt(&active_wars[i]);
            continue;
        }
        if (active_wars[i].attacker != attacker) {
            active_wars[i].attacker = attacker;
            active_wars[i].attacker_uid = civs[attacker].uid;
        }
        if (active_wars[i].defender != defender) {
            active_wars[i].defender = defender;
            active_wars[i].defender_uid = civs[defender].uid;
        }
    }
    support_casualties[to_civ] += support_casualties[from_civ];
    support_casualties[from_civ] = 0;
}
