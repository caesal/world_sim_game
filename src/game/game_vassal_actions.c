#include "game/game.h"

#include "core/country_focus.h"
#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "core/render_snapshot.h"
#include "core/render_snapshot_cache.h"
#include "core/state_lock.h"
#include "sim/decision_snapshot.h"
#include "sim/diplomacy.h"
#include "sim/maritime.h"
#include "sim/ports.h"
#include "sim/simulation.h"
#include "sim/vassal.h"

static void refresh_after_vassal_release(void) {
    world_invalidate_country_summary_cache();
    diplomacy_mark_contacts_dirty();
    diplomacy_update_contacts();
    maritime_mark_routes_dirty();
    dirty_mark_territory();
    decision_snapshot_cache_mark_all_dirty();
    world_visual_revision++;
    render_snapshot_cache_update_all();
}

static void refresh_after_vassal_annex(void) {
    world_invalidate_region_cache();
    ports_refresh_city_regions();
    dirty_mark_civ();
    country_focus_invalidate();
    refresh_after_vassal_release();
}

int game_request_release_vassal(int vassal_id) {
    int overlord;

    game_pause_for_modal_or_action();
    if (!world_generated || vassal_id < 0 || vassal_id >= civ_count || !civs[vassal_id].alive) return 0;
    state_write_lock();
    overlord = vassal_overlord(vassal_id);
    if (overlord < 0 || overlord >= civ_count || !civs[overlord].alive) {
        state_write_unlock();
        event_log_push_structured(EVENT_TYPE_DEBUG_NOTICE, EVENT_SEVERITY_WARNING,
                                  vassal_id, -1, -1, -1, 0, 0, "VASSAL_RELEASE_FAILED_NO_OVERLORD");
        return 0;
    }
    vassal_release(vassal_id);
    event_log_push_structured(EVENT_TYPE_VASSAL_RELEASED, EVENT_SEVERITY_INFO,
                              vassal_id, overlord, -1, -1, 0, 0, "");
    refresh_after_vassal_release();
    state_write_unlock();
    render_snapshot_publish_from_live_state();
    return 1;
}

int game_request_annex_vassal(int overlord_id, int vassal_id) {
    int annexed;

    game_pause_for_modal_or_action();
    if (!world_generated ||
        overlord_id < 0 || overlord_id >= civ_count || !civs[overlord_id].alive ||
        vassal_id < 0 || vassal_id >= civ_count || !civs[vassal_id].alive) {
        return 0;
    }
    state_write_lock();
    if (!vassal_is_direct(overlord_id, vassal_id)) {
        state_write_unlock();
        return 0;
    }
    annexed = vassal_annex_direct(overlord_id, vassal_id);
    if (annexed) refresh_after_vassal_annex();
    state_write_unlock();
    if (annexed) render_snapshot_publish_from_live_state();
    return annexed;
}
