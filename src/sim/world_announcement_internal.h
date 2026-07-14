#ifndef WORLD_SIM_WORLD_ANNOUNCEMENT_INTERNAL_H
#define WORLD_SIM_WORLD_ANNOUNCEMENT_INTERNAL_H

#include "core/world_announcement_types.h"

void world_announcement_event_init(WorldAnnouncementEvent *event, int event_type, int priority);
void world_announcement_capture_civ(WorldAnnouncementCivIdentity *identity, int civ_id);
void world_announcement_capture_alliance(WorldAnnouncementAllianceIdentity *identity,
                                         int alliance_id);
void world_announcement_add_related(WorldAnnouncementEvent *event, int civ_id);
void world_announcement_set_location_civ(WorldAnnouncementEvent *event, int civ_id);
void world_announcement_set_location_city(WorldAnnouncementEvent *event, int city_id);
void world_announcement_alliance_raw_name(int alliance_id, char *out, int out_size);
int world_announcement_publish(WorldAnnouncementEvent *event, int severity,
                               int log_civ_id, int log_target_id,
                               int region_id, int city_id,
                               int param_a, int param_b, const char *raw_message);
int world_announcement_publish_plague(WorldAnnouncementEvent *event, int severity,
                                      const PlagueEventPayload *payload);

#endif
