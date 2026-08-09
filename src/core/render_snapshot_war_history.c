#include "core/render_snapshot_war_history.h"

#include "sim/war_history.h"

#include <string.h>

void render_snapshot_war_history_copy(SnapshotWarHistory *out,
                                      int civ_id,
                                      int expected_uid) {
    SnapshotWarHistory history;
    if (!out) return;
    memset(&history, 0, sizeof(history));
    if (war_history_copy_for_civ(civ_id, expected_uid, &history) &&
        history.owner_uid == expected_uid) {
        *out = history;
        return;
    }
    memset(out, 0, sizeof(*out));
}
