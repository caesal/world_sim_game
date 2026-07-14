#ifndef WORLD_SIM_EVENT_LOG_PLAGUE_H
#define WORLD_SIM_EVENT_LOG_PLAGUE_H

#include "core/plague_event_types.h"

#include <stddef.h>

int event_log_plague_format_payload(const PlagueEventPayload *payload,
                                    int language, char *out, size_t out_size);

#endif
