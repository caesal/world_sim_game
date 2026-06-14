#include "sim/war.h"

#include "sim/diplomacy.h"
#include "sim/war_internal.h"

#include <string.h>

int war_end_direct_for_civ_no_winner(int civ_id, int last_war_result,
                                     int truce_years, int relation_score) {
    int i;
    int ended = 0;
    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return 0;
    for (i = 0; i < MAX_ACTIVE_WARS; i++) {
        ActiveWar *war = &active_wars[i];
        int attacker;
        int defender;
        if (!war->active) continue;
        if (war->attacker != civ_id && war->defender != civ_id) continue;
        attacker = war->attacker;
        defender = war->defender;
        diplomacy_record_war_no_winner(attacker, defender,
                                       (DiplomacyLastWarResult)last_war_result);
        diplomacy_start_truce(attacker, defender, truce_years, relation_score);
        memset(war, 0, sizeof(*war));
        ended++;
    }
    return ended;
}
