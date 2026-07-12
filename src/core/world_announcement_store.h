#ifndef WORLD_SIM_WORLD_ANNOUNCEMENT_STORE_H
#define WORLD_SIM_WORLD_ANNOUNCEMENT_STORE_H

#include "core/world_announcement_types.h"

void world_announcement_store_clear(void);
int world_announcement_store_append(const WorldAnnouncementEvent *event);
int world_announcement_store_count(void);
int world_announcement_store_total_entries(void);
int world_announcement_store_copy_newest(WorldAnnouncementEvent *out, int max_events);
int world_announcement_store_copy_newest_stream(WorldAnnouncementStreamEntry *out,
                                                int max_events);
int world_announcement_store_get_by_event_id(int event_id, WorldAnnouncementEvent *out);

#endif
