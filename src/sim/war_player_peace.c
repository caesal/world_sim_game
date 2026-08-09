#include "sim/war.h"

#include "sim/war_terminal.h"

int war_end_direct_for_civ_no_winner(int civ_id, int last_war_result,
                                     int truce_years, int relation_score) {
    return war_terminal_end_direct_for_civ_no_winner(
        civ_id, last_war_result, truce_years, relation_score);
}
