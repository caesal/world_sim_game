#ifndef WORLD_SIM_EVENT_LOG_HISTORY_H
#define WORLD_SIM_EVENT_LOG_HISTORY_H

#include "core/game_types.h"

void event_log_history_clear(void);
void event_log_history_store_related(const EventLogEntry *entry);
void event_log_history_store_related_id(const EventLogEntry *entry, int event_id);

#endif
