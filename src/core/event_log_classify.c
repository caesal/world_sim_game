#include "core/event_log_classify.h"

#include <string.h>

EventLogType event_log_type_from_text(const char *text) {
    if (!text) return EVENT_TYPE_GENERIC;
    if (strstr(text, "[Performance]")) return EVENT_TYPE_PERFORMANCE_THROTTLED;
    if (strstr(text, "[Disorder]")) return EVENT_TYPE_DISORDER_CHANGED;
    if (strstr(text, "[World]")) return EVENT_TYPE_WORLD_GENERATION_NOTICE;
    if (strstr(text, "[Debug]") || strstr(text, "[Render]")) return EVENT_TYPE_DEBUG_NOTICE;
    if (strstr(text, "[War] War started")) return EVENT_TYPE_WAR_STARTED;
    if (strstr(text, "[War]")) return EVENT_TYPE_BATTLE_RESOLVED;
    if (strstr(text, "[Collapse] Collapse succeeded") || strstr(text, "Collapse succeeded")) {
        return EVENT_TYPE_COLLAPSE_SUCCEEDED;
    }
    if (strstr(text, "[Collapse]") || strstr(text, "Collapse failed") || strstr(text, "Collapse blocked")) {
        return EVENT_TYPE_COLLAPSE_FAILED;
    }
    if (strstr(text, "[Plague]") && strstr(text, "ended")) return EVENT_TYPE_PLAGUE_ENDED;
    if (strstr(text, "[Plague]") && strstr(text, "spread")) return EVENT_TYPE_PLAGUE_SPREAD;
    if (strstr(text, "[Plague]") || strstr(text, "Plague outbreak")) return EVENT_TYPE_PLAGUE_STARTED;
    if (strstr(text, "became vassal")) return EVENT_TYPE_VASSAL_CREATED;
    if (strstr(text, "vassal") && strstr(text, "independent")) return EVENT_TYPE_VASSAL_RELEASED;
    if (strstr(text, "deep sea") && strstr(text, "failed")) return EVENT_TYPE_DEEP_SEA_ROUTE_FAILED;
    if (strstr(text, "deep sea")) return EVENT_TYPE_DEEP_SEA_ROUTE_CREATED;
    if (strstr(text, "Civil unrest")) return EVENT_TYPE_CIVIL_UNREST_TRIGGERED;
    if (strstr(text, "Truce")) return EVENT_TYPE_TRUCE_SIGNED;
    if (strstr(text, "created country")) return EVENT_TYPE_CIV_CREATED;
    return EVENT_TYPE_GENERIC;
}

EventLogSeverity event_log_severity_from_type(EventLogType type) {
    switch (type) {
        case EVENT_TYPE_WAR_STARTED:
        case EVENT_TYPE_WAR_FRONT_SEVERED:
        case EVENT_TYPE_BATTLE_RESOLVED:
        case EVENT_TYPE_VASSAL_INDEPENDENCE_WAR:
        case EVENT_TYPE_TREASURY_INDEMNITY:
        case EVENT_TYPE_MERCENARIES_HIRED:
        case EVENT_TYPE_COLLAPSE_FAILED:
        case EVENT_TYPE_PLAGUE_STARTED:
        case EVENT_TYPE_PERFORMANCE_THROTTLED:
        case EVENT_TYPE_PERFORMANCE_SLOW_CALL:
        case EVENT_TYPE_SCHEDULER_YIELD:
            return EVENT_SEVERITY_WARNING;
        case EVENT_TYPE_COLLAPSE_SUCCEEDED:
        case EVENT_TYPE_CIVIL_UNREST_TRIGGERED:
        case EVENT_TYPE_VASSAL_ANNEXED:
        case EVENT_TYPE_ENCLAVE_FAILED:
            return EVENT_SEVERITY_DANGER;
        default:
            return EVENT_SEVERITY_INFO;
    }
}
