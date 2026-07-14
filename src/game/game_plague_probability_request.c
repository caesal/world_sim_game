#include "game/game_plague_probability_request.h"

#include "core/dirty_flags.h"
#include "core/render_snapshot.h"
#include "core/state_lock.h"
#include "sim/plague_probability.h"
#include "sim/plague_state.h"

int game_request_apply_plague_probabilities(
    const PlagueProbabilityDistribution *probabilities) {
    int applied;

    if (!plague_probability_validate(probabilities)) return 0;
    state_write_lock();
    applied = plague_state_apply_probabilities(probabilities);
    if (applied) {
        dirty_mark_plague_configuration();
    }
    state_write_unlock();
    if (applied) render_snapshot_publish_from_live_state();
    return applied;
}
