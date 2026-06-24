#include "sim/war.h"

#include "sim/war_internal.h"

#include <string.h>

void war_transfer_civ_identity(int from_civ, int to_civ) {
    int i;
    if (from_civ < 0 || from_civ >= MAX_CIVS || to_civ < 0 || to_civ >= MAX_CIVS ||
        from_civ == to_civ) return;
    for (i = 0; i < MAX_ACTIVE_WARS; i++) {
        if (!active_wars[i].active) continue;
        if (active_wars[i].attacker == from_civ) active_wars[i].attacker = to_civ;
        if (active_wars[i].defender == from_civ) active_wars[i].defender = to_civ;
        if (active_wars[i].attacker == active_wars[i].defender) memset(&active_wars[i], 0, sizeof(active_wars[i]));
    }
    support_casualties[to_civ] += support_casualties[from_civ];
    support_casualties[from_civ] = 0;
}
