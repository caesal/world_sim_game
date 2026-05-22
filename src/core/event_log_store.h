#ifndef WORLD_SIM_EVENT_LOG_STORE_H
#define WORLD_SIM_EVENT_LOG_STORE_H

#include "core/game_types.h"

void event_log_store_clear(void);
int event_log_store_append(const EventLogEntry *entry);
int event_log_store_increment_repeat(int event_id);
int event_log_store_count(void);
int event_log_store_get_newest(int index, EventLogEntry *out);
int event_log_store_get_oldest(int index, EventLogEntry *out);
int event_log_store_get_by_id(int event_id, EventLogEntry *out);

#endif
