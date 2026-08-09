#ifndef WORLD_SIM_RENDER_SNAPSHOT_WAR_HISTORY_H
#define WORLD_SIM_RENDER_SNAPSHOT_WAR_HISTORY_H

#include "sim/war_history_types.h"

typedef WarHistoryPrincipal SnapshotWarHistoryPrincipal;
typedef WarHistoryRecord SnapshotWarHistoryRecord;
typedef WarHistory SnapshotWarHistory;

void render_snapshot_war_history_copy(SnapshotWarHistory *out,
                                      int civ_id,
                                      int expected_uid);

#endif
