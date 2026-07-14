#include "core/event_log_plague.h"

#include <stdio.h>

static const char *size_label(int size, int language) {
    if (language != 0) {
        if (size == 1) return "小型";
        if (size == 2) return "中型";
        if (size == 3) return "大型";
        return "未知规模";
    }
    if (size == 1) return "Small";
    if (size == 2) return "Medium";
    if (size == 3) return "Large";
    return "Unknown-size";
}

int event_log_plague_format_payload(const PlagueEventPayload *payload,
                                    int language, char *out, size_t out_size) {
    const char *name;
    const char *size;
    int written;
    if (!out || out_size == 0) return 0;
    out[0] = '\0';
    if (!payload || !payload->valid) return 0;
    name = language != 0 ? payload->name_zh : payload->name_en;
    size = size_label(payload->size, language);
    if (payload->kind == PLAGUE_EVENT_START) {
        if (language != 0) {
            written = snprintf(out, out_size, "%s瘟疫“%s”在%s暴发。",
                               size, name, payload->origin_city_name);
        } else {
            written = snprintf(out, out_size, "%s plague \"%s\" broke out in %s.",
                               size, name, payload->origin_city_name);
        }
    } else if (payload->kind == PLAGUE_EVENT_END) {
        if (language != 0) {
            written = snprintf(
                out, out_size,
                "%s瘟疫“%s”在持续%d个月后结束；波及%d座城市、%d个国家，造成%lld人死亡。",
                size, name, payload->duration_months, payload->affected_city_count,
                payload->affected_country_count, (long long)payload->total_deaths);
        } else {
            const char *city_word = payload->affected_city_count == 1 ? "city" : "cities";
            const char *country_word = payload->affected_country_count == 1 ? "country" : "countries";
            written = snprintf(
                out, out_size,
                "%s plague \"%s\" ended after %d months; %d %s and %d %s "
                "were affected, with %lld deaths.",
                size, name, payload->duration_months, payload->affected_city_count,
                city_word, payload->affected_country_count, country_word,
                (long long)payload->total_deaths);
        }
    } else {
        return 0;
    }
    return written >= 0 && (size_t)written < out_size;
}
