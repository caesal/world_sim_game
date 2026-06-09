#ifndef WORLD_SIM_EVENT_LOG_CLASSIFY_H
#define WORLD_SIM_EVENT_LOG_CLASSIFY_H

#include "core/game_types.h"

EventLogType event_log_type_from_text(const char *text);
EventLogSeverity event_log_severity_from_type(EventLogType type);

#endif
