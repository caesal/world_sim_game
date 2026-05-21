#include "game/game.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "core/render_snapshot.h"
#include "core/state_lock.h"
#include "sim/collapse.h"
#include "sim/diplomacy.h"
#include "sim/disorder.h"
#include "sim/maritime.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/vassal.h"

int game_request_trigger_civil_unrest(int civ_id) {
    int collapsed;
    game_pause_for_modal_or_action();
    if (!world_generated || civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) {
        event_log_push_structured(EVENT_TYPE_DEBUG_NOTICE, EVENT_SEVERITY_WARNING,
                                  -1, -1, -1, -1, 0, 0, "Civil unrest failed: invalid country.");
        return 0;
    }
    state_write_lock();
    disorder_set_civil_unrest(civ_id);
    event_log_push_structured(EVENT_TYPE_CIVIL_UNREST_TRIGGERED, EVENT_SEVERITY_DANGER,
                              civ_id, -1, -1, -1, 100, 0, "");
    collapsed = collapse_check_immediate(civ_id, COLLAPSE_CAUSE_CIVIL_UNREST);
    (void)collapsed;
    world_invalidate_region_cache();
    ports_refresh_city_regions();
    maritime_mark_routes_dirty();
    diplomacy_mark_contacts_dirty();
    diplomacy_update_contacts();
    dirty_mark_territory();
    dirty_mark_labels();
    world_visual_revision++;
    state_write_unlock();
    render_snapshot_publish_from_live_state();
    return collapsed;
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
    world_invalidate_country_summary_cache();
    diplomacy_mark_contacts_dirty();
    diplomacy_update_contacts();
    maritime_mark_routes_dirty();
    dirty_mark_territory();
    dirty_mark_labels();
    world_visual_revision++;
    state_write_unlock();
    render_snapshot_publish_from_live_state();
    return 1;
}
