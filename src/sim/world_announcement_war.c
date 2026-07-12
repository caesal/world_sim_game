#include "sim/world_announcement.h"
#include "sim/world_announcement_internal.h"

#include "core/game_types.h"
#include "sim/alliance.h"
#include "sim/diplomacy.h"
#include "sim/war.h"
#include "sim/war_internal.h"

#include <string.h>

typedef struct {
    int active;
    int terminal_emitted;
    int war_id;
    int war_class;
    WorldAnnouncementCivIdentity actor;
    WorldAnnouncementCivIdentity target;
    WorldAnnouncementAllianceIdentity alliance_a;
    WorldAnnouncementAllianceIdentity alliance_b;
} AnnouncementWarRecord;

static AnnouncementWarRecord records[MAX_ACTIVE_WARS];
static int next_war_id = 1;

static int military_alliance_for_civ(int civ_id) {
    int alliance_id = alliance_for_civ(civ_id);
    AllianceSaveState *state = alliance_internal_state();
    return state && alliance_id >= 0 && alliance_id < state->next_id &&
           alliance_id < ALLIANCE_MAX && state->records[alliance_id].active &&
           alliance_type(alliance_id) == ALLIANCE_TYPE_MILITARY ? alliance_id : -1;
}

static void register_war(int slot, int attacker, int defender, int emit) {
    AnnouncementWarRecord *record;
    int alliance_a;
    int alliance_b;
    if (slot < 0 || slot >= MAX_ACTIVE_WARS) return;
    record = &records[slot];
    memset(record, 0, sizeof(*record));
    record->active = 1;
    record->war_id = next_war_id++;
    world_announcement_capture_civ(&record->actor, attacker);
    world_announcement_capture_civ(&record->target, defender);
    alliance_a = military_alliance_for_civ(attacker);
    alliance_b = military_alliance_for_civ(defender);
    world_announcement_capture_alliance(&record->alliance_a, alliance_a);
    world_announcement_capture_alliance(&record->alliance_b, alliance_b);
    if (alliance_a >= 0 && alliance_b >= 0 && alliance_a != alliance_b) {
        record->war_class = WORLD_ANNOUNCEMENT_WAR_ALLIANCE_VS_ALLIANCE;
    } else if ((alliance_a >= 0) != (alliance_b >= 0)) {
        record->war_class = WORLD_ANNOUNCEMENT_WAR_ALLIANCE_VS_COUNTRY;
    }
    if (emit && record->war_class != WORLD_ANNOUNCEMENT_WAR_NONE) {
        WorldAnnouncementEvent event;
        world_announcement_event_init(&event, EVENT_TYPE_ALLIANCE_WAR_STARTED,
                                      WORLD_ANNOUNCEMENT_NORMAL);
        event.actor = record->actor;
        event.target = record->target;
        event.alliance_a = record->alliance_a;
        event.alliance_b = record->alliance_b;
        event.war_id = record->war_id;
        event.war_class = record->war_class;
        world_announcement_set_location_civ(&event, attacker);
        world_announcement_publish(&event, EVENT_SEVERITY_WARNING, attacker, defender,
                                   -1, -1, record->war_id, record->war_class, "");
    }
}

void world_announcement_war_reset(void) {
    memset(records, 0, sizeof(records));
    next_war_id = 1;
}

void world_announcement_war_baseline_active(void) {
    int i;
    world_announcement_war_reset();
    for (i = 0; i < MAX_ACTIVE_WARS; i++) {
        if (active_wars[i].active) {
            AnnouncementWarRecord *record = &records[i];
            record->active = 1;
            record->war_id = next_war_id++;
            world_announcement_capture_civ(&record->actor, active_wars[i].attacker);
            world_announcement_capture_civ(&record->target, active_wars[i].defender);
            /* The save format has no war-start alliance identity; do not infer one at load. */
        }
    }
}

void world_announcement_war_started(int slot, int attacker, int defender) {
    register_war(slot, attacker, defender, 1);
}

void world_announcement_war_ended(int slot, int outcome, int last_war_result) {
    AnnouncementWarRecord *record;
    WorldAnnouncementEvent event;
    if (slot < 0 || slot >= MAX_ACTIVE_WARS) return;
    record = &records[slot];
    if (!record->active || record->terminal_emitted) return;
    record->terminal_emitted = 1;
    if (record->war_class == WORLD_ANNOUNCEMENT_WAR_NONE) {
        record->active = 0;
        return;
    }
    world_announcement_event_init(&event, EVENT_TYPE_ALLIANCE_WAR_ENDED,
                                  WORLD_ANNOUNCEMENT_NORMAL);
    event.actor = record->actor;
    event.target = record->target;
    event.alliance_a = record->alliance_a;
    event.alliance_b = record->alliance_b;
    event.war_id = record->war_id;
    event.war_class = record->war_class;
    if (outcome == WAR_OUTCOME_ATTACKER_WIN || outcome == WAR_OUTCOME_DEFENDER_WIN) {
        event.terminal_result = WORLD_ANNOUNCEMENT_TERMINAL_VICTORY;
        event.winner_side = outcome == WAR_OUTCOME_ATTACKER_WIN ? 1 : 2;
    } else if (last_war_result == DIP_LAST_WAR_OFFENSIVE_HALTED) {
        event.terminal_result = WORLD_ANNOUNCEMENT_TERMINAL_OFFENSIVE_HALTED;
    } else if (last_war_result == DIP_LAST_WAR_FRONT_SEVERED) {
        event.terminal_result = WORLD_ANNOUNCEMENT_TERMINAL_FRONT_SEVERED;
    } else {
        event.terminal_result = WORLD_ANNOUNCEMENT_TERMINAL_NEGOTIATED_TRUCE;
    }
    world_announcement_set_location_civ(&event,
        event.winner_side == 2 ? record->target.civ_id : record->actor.civ_id);
    world_announcement_publish(&event, EVENT_SEVERITY_WARNING,
                               record->actor.civ_id, record->target.civ_id,
                               -1, -1, record->war_id, event.terminal_result, "");
    record->active = 0;
}
