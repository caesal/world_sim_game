#include "sim/world_announcement.h"
#include "sim/world_announcement_internal.h"

#include "core/event_log_classify.h"
#include "core/game_types.h"
#include "core/world_announcement_store.h"
#include "sim/alliance.h"
#include "sim/simulation.h"

#include <stdio.h>
#include <string.h>

static unsigned int announced_age_mask;
static int deep_sea_announced;

static int active_alliance_id(int alliance_id) {
    AllianceSaveState *state = alliance_internal_state();
    return state && alliance_id >= 0 && alliance_id < state->next_id &&
           alliance_id < ALLIANCE_MAX && state->records[alliance_id].active;
}

void world_announcement_event_init(WorldAnnouncementEvent *event, int event_type, int priority) {
    if (!event) return;
    memset(event, 0, sizeof(*event));
    event->event_type = event_type;
    event->priority = priority;
    event->location_civ_id = -1;
    event->location_city_id = -1;
    event->location_region_id = -1;
    event->location_x = -1;
    event->location_y = -1;
    event->actor.civ_id = -1;
    event->target.civ_id = -1;
}

void world_announcement_capture_civ(WorldAnnouncementCivIdentity *identity, int civ_id) {
    if (!identity) return;
    memset(identity, 0, sizeof(*identity));
    identity->civ_id = civ_id;
    if (civ_id < 0 || civ_id >= civ_count) return;
    identity->uid = civs[civ_id].uid;
    identity->symbol = civs[civ_id].symbol;
    identity->color = civs[civ_id].color;
    snprintf(identity->name_en, sizeof(identity->name_en), "%s",
             civilization_display_name_for_language(civ_id, 0));
    snprintf(identity->name_zh, sizeof(identity->name_zh), "%s",
             civilization_display_name_for_language(civ_id, 1));
}

void world_announcement_capture_alliance(WorldAnnouncementAllianceIdentity *identity,
                                         int alliance_id) {
    if (!identity) return;
    memset(identity, 0, sizeof(*identity));
    identity->alliance_id = alliance_id;
    if (!active_alliance_id(alliance_id)) return;
    identity->valid = 1;
    identity->leader_civ_id = alliance_founder(alliance_id);
    identity->color = alliance_color(alliance_id);
    snprintf(identity->name_en, sizeof(identity->name_en), "%s", alliance_name_en(alliance_id));
    snprintf(identity->name_zh, sizeof(identity->name_zh), "%s", alliance_name_zh(alliance_id));
}

void world_announcement_add_related(WorldAnnouncementEvent *event, int civ_id) {
    int i;
    if (!event || civ_id < 0 || civ_id >= civ_count || event->related_count >= MAX_CIVS) return;
    for (i = 0; i < event->related_count; i++) {
        if (event->related[i].civ_id == civ_id && event->related[i].uid == civs[civ_id].uid) return;
    }
    world_announcement_capture_civ(&event->related[event->related_count++], civ_id);
}

void world_announcement_set_location_civ(WorldAnnouncementEvent *event, int civ_id) {
    if (!event || civ_id < 0 || civ_id >= civ_count) return;
    event->location_civ_id = civ_id;
    event->location_civ_uid = civs[civ_id].uid;
    if (civs[civ_id].capital_city >= 0) world_announcement_set_location_city(event, civs[civ_id].capital_city);
}

void world_announcement_set_location_city(WorldAnnouncementEvent *event, int city_id) {
    if (!event || city_id < 0 || city_id >= city_count) return;
    event->location_city_id = city_id;
    event->location_x = cities[city_id].x;
    event->location_y = cities[city_id].y;
    snprintf(event->location_name_en, sizeof(event->location_name_en), "%s", cities[city_id].name);
    snprintf(event->location_name_zh, sizeof(event->location_name_zh), "%s", cities[city_id].name);
}

void world_announcement_alliance_raw_name(int alliance_id, char *out, int out_size) {
    if (!out || out_size <= 0) return;
    if (active_alliance_id(alliance_id)) {
        snprintf(out, (size_t)out_size, "%s\t%s", alliance_name_en(alliance_id),
                 alliance_name_zh(alliance_id));
    } else {
        out[0] = '\0';
    }
}

int world_announcement_publish(WorldAnnouncementEvent *event, int severity,
                               int log_civ_id, int log_target_id,
                               int region_id, int city_id,
                               int param_a, int param_b, const char *raw_message) {
    int event_id;
    if (!event) return 0;
    event_id = event_log_push_structured_id((EventLogType)event->event_type,
                                            (EventLogSeverity)severity,
                                            log_civ_id, log_target_id, region_id, city_id,
                                            param_a, param_b, raw_message);
    if (event_id <= 0) return 0;
    event->event_id = event_id;
    event->year = year;
    event->month = month;
    return world_announcement_store_append(event);
}

int world_announcement_publish_plague(WorldAnnouncementEvent *event, int severity,
                                      const PlagueEventPayload *payload) {
    int event_id;
    if (!event || !payload || !payload->valid) return 0;
    event_id = event_log_push_plague_event((EventLogType)event->event_type,
                                           (EventLogSeverity)severity, payload);
    if (event_id <= 0) return 0;
    event->event_id = event_id;
    event->year = year;
    event->month = month;
    event->plague = *payload;
    return world_announcement_store_append(event);
}

void world_announcement_state_reset(void) {
    announced_age_mask = 0;
    deep_sea_announced = 0;
    world_announcement_war_reset();
}

void world_announcement_state_baseline_from_world(void) {
    int i;
    int max_stage = 0;
    for (i = 0; i < civ_count; i++) {
        if (!civs[i].alive) continue;
        if (civs[i].tech_stage > max_stage) max_stage = civs[i].tech_stage;
        if (civs[i].deep_sea_route_unlocked_event_done) deep_sea_announced = 1;
    }
    for (i = 1; i <= clamp(max_stage, 0, 10); i++) announced_age_mask |= 1u << i;
    if (max_stage >= 6) deep_sea_announced = 1;
    world_announcement_war_baseline_active();
}

void world_announcement_emit_age_first(int civ_id, int stage) {
    WorldAnnouncementEvent event;
    unsigned int bit;
    if (stage <= 0 || stage > 10 || civ_id < 0 || civ_id >= civ_count) return;
    bit = 1u << stage;
    if (announced_age_mask & bit) return;
    announced_age_mask |= bit;
    if (stage == 6) deep_sea_announced = 1;
    world_announcement_event_init(&event, EVENT_TYPE_WORLD_TECH_AGE_FIRST,
                                  WORLD_ANNOUNCEMENT_CRITICAL);
    world_announcement_capture_civ(&event.actor, civ_id);
    event.technology_stage = stage;
    world_announcement_set_location_civ(&event, civ_id);
    world_announcement_publish(&event, EVENT_SEVERITY_DANGER, civ_id, -1,
                               -1, -1, stage, 0, "");
}

void world_announcement_emit_deep_sea_first(int civ_id) {
    WorldAnnouncementEvent event;
    if (deep_sea_announced || civ_id < 0 || civ_id >= civ_count) return;
    deep_sea_announced = 1;
    if (announced_age_mask & (1u << 6)) return;
    world_announcement_event_init(&event, EVENT_TYPE_WORLD_DEEP_SEA_FIRST,
                                  WORLD_ANNOUNCEMENT_CRITICAL);
    world_announcement_capture_civ(&event.actor, civ_id);
    world_announcement_set_location_civ(&event, civ_id);
    world_announcement_publish(&event, EVENT_SEVERITY_DANGER, civ_id, -1,
                               -1, -1, 0, 0, "");
}

void world_announcement_emit_collapse(int civ_id, int target_id, int region_id,
                                      const int *successor_ids, int successor_count,
                                      int event_param) {
    WorldAnnouncementEvent event;
    int i;
    world_announcement_event_init(&event, EVENT_TYPE_COLLAPSE_SUCCEEDED,
                                  WORLD_ANNOUNCEMENT_CRITICAL);
    world_announcement_capture_civ(&event.actor, civ_id);
    world_announcement_capture_civ(&event.target, target_id);
    for (i = 0; successor_ids && i < successor_count; i++) {
        world_announcement_add_related(&event, successor_ids[i]);
    }
    if (target_id >= 0) world_announcement_set_location_civ(&event, target_id);
    else if (event.related_count > 0) world_announcement_set_location_civ(&event, event.related[0].civ_id);
    else world_announcement_set_location_civ(&event, civ_id);
    event.location_region_id = region_id;
    world_announcement_publish(&event, EVENT_SEVERITY_DANGER, civ_id, target_id,
                               region_id, -1, event_param, 0, "");
}

void world_announcement_emit_vassal(int event_type, int vassal_id,
                                    int overlord_id, int other_overlord_id,
                                    int event_param) {
    world_announcement_emit_vassal_detail(event_type, vassal_id, overlord_id,
                                          other_overlord_id, -1, -1, event_param, 0);
}

void world_announcement_emit_vassal_detail(int event_type, int vassal_id,
                                           int overlord_id, int other_overlord_id,
                                           int region_id, int city_id,
                                           int param_a, int param_b) {
    WorldAnnouncementEvent event;
    int priority = event_type == EVENT_TYPE_VASSAL_INDEPENDENCE_WAR ||
                   event_type == EVENT_TYPE_VASSAL_ANNEXED ?
                   WORLD_ANNOUNCEMENT_CRITICAL : WORLD_ANNOUNCEMENT_MAJOR;
    world_announcement_event_init(&event, event_type, priority);
    world_announcement_capture_civ(&event.actor, vassal_id);
    world_announcement_capture_civ(&event.target, overlord_id);
    if (other_overlord_id >= 0) world_announcement_add_related(&event, other_overlord_id);
    world_announcement_set_location_civ(&event,
        event_type == EVENT_TYPE_VASSAL_ANNEXED ? overlord_id : vassal_id);
    event.location_region_id = region_id;
    if (city_id >= 0) world_announcement_set_location_city(&event, city_id);
    world_announcement_publish(&event, event_log_severity_from_type((EventLogType)event_type),
                               vassal_id, overlord_id, region_id, city_id,
                               param_a, param_b, "");
}
