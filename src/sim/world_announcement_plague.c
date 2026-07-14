#include "sim/world_announcement_plague.h"

#include "core/event_log_types.h"
#include "core/plague_event_types.h"
#include "data/plague_names.h"
#include "sim/world_announcement_internal.h"

#include <stdio.h>
#include <string.h>

static void copy_origin_from_episode(PlagueEventPayload *payload,
                                     const PlagueEpisodeState *episode) {
    payload->origin_city_id = episode->origin_city_id;
    payload->origin_civ_id = episode->origin_civ_id;
    payload->origin_civ_uid = episode->origin_civ_uid;
    payload->origin_civ_symbol = episode->origin_civ_symbol;
    payload->origin_civ_color = episode->origin_civ_color;
    snprintf(payload->origin_city_name, sizeof(payload->origin_city_name), "%s",
             episode->origin_city_name);
    snprintf(payload->origin_civ_name_en, sizeof(payload->origin_civ_name_en), "%s",
             episode->origin_civ_name_en);
    snprintf(payload->origin_civ_name_zh, sizeof(payload->origin_civ_name_zh), "%s",
             episode->origin_civ_name_zh);
}

static void copy_origin_from_history(PlagueEventPayload *payload,
                                     const PlagueEpisodeHistory *history) {
    payload->origin_city_id = history->origin_city_id;
    payload->origin_civ_id = history->origin_civ_id;
    payload->origin_civ_uid = history->origin_civ_uid;
    payload->origin_civ_symbol = history->origin_civ_symbol;
    payload->origin_civ_color = history->origin_civ_color;
    snprintf(payload->origin_city_name, sizeof(payload->origin_city_name), "%s",
             history->origin_city_name);
    snprintf(payload->origin_civ_name_en, sizeof(payload->origin_civ_name_en), "%s",
             history->origin_civ_name_en);
    snprintf(payload->origin_civ_name_zh, sizeof(payload->origin_civ_name_zh), "%s",
             history->origin_civ_name_zh);
}

static int finish_payload_names(PlagueEventPayload *payload) {
    return plague_names_format(payload->name_id, payload->name_cycle, 0,
                               payload->name_en, sizeof(payload->name_en)) &&
           plague_names_format(payload->name_id, payload->name_cycle, 1,
                               payload->name_zh, sizeof(payload->name_zh));
}

static void apply_origin_identity(WorldAnnouncementEvent *event,
                                  const PlagueEventPayload *payload) {
    event->actor.civ_id = payload->origin_civ_id;
    event->actor.uid = payload->origin_civ_uid;
    event->actor.symbol = payload->origin_civ_symbol;
    event->actor.color = payload->origin_civ_color;
    snprintf(event->actor.name_en, sizeof(event->actor.name_en), "%s",
             payload->origin_civ_name_en);
    snprintf(event->actor.name_zh, sizeof(event->actor.name_zh), "%s",
             payload->origin_civ_name_zh);
    event->location_civ_id = payload->origin_civ_id;
    event->location_civ_uid = payload->origin_civ_uid;
    event->location_city_id = payload->origin_city_id;
    snprintf(event->location_name_en, sizeof(event->location_name_en), "%s",
             payload->origin_city_name);
    snprintf(event->location_name_zh, sizeof(event->location_name_zh), "%s",
             payload->origin_city_name);
}

static int publish_payload(const PlagueEventPayload *payload, int event_type,
                           int severity) {
    WorldAnnouncementEvent event;
    world_announcement_event_init(&event, event_type, WORLD_ANNOUNCEMENT_MAJOR);
    apply_origin_identity(&event, payload);
    return world_announcement_publish_plague(&event, severity, payload);
}

int world_announcement_plague_emit_start(PlagueEpisodeState *episode) {
    PlagueEventPayload payload;
    if (!episode || !episode->active || episode->episode_id <= 0 ||
        episode->start_event_emitted) return 0;
    memset(&payload, 0, sizeof(payload));
    payload.valid = 1;
    payload.kind = PLAGUE_EVENT_START;
    payload.episode_id = episode->episode_id;
    payload.stable_event_id = plague_event_stable_id(payload.episode_id, payload.kind);
    payload.size = episode->size;
    payload.severity = episode->severity;
    payload.name_id = episode->name_id;
    payload.name_cycle = episode->name_cycle;
    payload.affected_city_count = 1;
    payload.affected_country_count = 1;
    copy_origin_from_episode(&payload, episode);
    if (!finish_payload_names(&payload)) return 0;
    if (!publish_payload(&payload, EVENT_TYPE_PLAGUE_STARTED,
                         EVENT_SEVERITY_WARNING)) return 0;
    episode->start_event_emitted = 1;
    return 1;
}

int world_announcement_plague_emit_end(PlagueEpisodeState *episode,
                                       const PlagueEpisodeHistory *history) {
    PlagueEventPayload payload;
    if (!episode || !history || history->episode_id <= 0 ||
        episode->episode_id != history->episode_id || episode->end_event_emitted) return 0;
    memset(&payload, 0, sizeof(payload));
    payload.valid = 1;
    payload.kind = PLAGUE_EVENT_END;
    payload.episode_id = history->episode_id;
    payload.stable_event_id = plague_event_stable_id(payload.episode_id, payload.kind);
    payload.size = history->size;
    payload.severity = history->severity;
    payload.name_id = history->name_id;
    payload.name_cycle = history->name_cycle;
    payload.duration_months = history->duration_months;
    payload.affected_city_count = history->infected_city_count;
    payload.affected_country_count = history->affected_country_count;
    payload.total_deaths = history->total_deaths;
    copy_origin_from_history(&payload, history);
    if (!finish_payload_names(&payload)) return 0;
    if (!publish_payload(&payload, EVENT_TYPE_PLAGUE_ENDED,
                         EVENT_SEVERITY_INFO)) return 0;
    episode->end_event_emitted = 1;
    return 1;
}
