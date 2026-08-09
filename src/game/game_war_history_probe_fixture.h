#ifndef WORLD_SIM_GAME_WAR_HISTORY_PROBE_FIXTURE_H
#define WORLD_SIM_GAME_WAR_HISTORY_PROBE_FIXTURE_H

#include "sim/war.h"
#include "sim/war_history_types.h"

typedef enum {
    WAR_HISTORY_PROBE_SETTLEMENT_NONE,
    WAR_HISTORY_PROBE_SETTLEMENT_CESSION,
    WAR_HISTORY_PROBE_SETTLEMENT_INDEMNITY,
    WAR_HISTORY_PROBE_SETTLEMENT_BOTH
} WarHistoryProbeSettlement;

void war_history_probe_fixture_reset(int civilization_count);
void war_history_probe_fixture_set_calendar(int current_year, int current_month);
ActiveWar *war_history_probe_fixture_active(int attacker, int defender,
                                            int start_absolute_month);
WarHistoryRecord war_history_probe_fixture_record(int local_civ, int opponent_civ,
                                                  uint64_t serial, int result,
                                                  int winner_uid, int loser_uid);
ActiveWar *war_history_probe_fixture_settlement(WarHistoryProbeSettlement settlement);
int war_history_probe_owned_regions(int civ_id);

#endif
