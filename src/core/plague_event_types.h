#ifndef WORLD_SIM_PLAGUE_EVENT_TYPES_H
#define WORLD_SIM_PLAGUE_EVENT_TYPES_H

#include "core/constants.h"
#include "core/value_types.h"

#include <stdint.h>

typedef enum {
    PLAGUE_EVENT_NONE = 0,
    PLAGUE_EVENT_START = 1,
    PLAGUE_EVENT_END = 2
} PlagueEventKind;

typedef struct {
    int valid;
    PlagueEventKind kind;
    uint64_t stable_event_id;
    int episode_id;
    int size;
    int severity;
    int name_id;
    int name_cycle;
    char name_en[NAME_LEN];
    char name_zh[NAME_LEN];
    int origin_city_id;
    int origin_civ_id;
    int origin_civ_uid;
    char origin_city_name[NAME_LEN];
    char origin_civ_name_en[NAME_LEN];
    char origin_civ_name_zh[NAME_LEN];
    char origin_civ_symbol;
    Color32 origin_civ_color;
    int duration_months;
    int affected_city_count;
    int affected_country_count;
    int64_t total_deaths;
} PlagueEventPayload;

static inline uint64_t plague_event_stable_id(int episode_id,
                                               PlagueEventKind kind) {
    if (episode_id <= 0 || (kind != PLAGUE_EVENT_START && kind != PLAGUE_EVENT_END)) {
        return 0;
    }
    return (uint64_t)(unsigned int)episode_id * UINT64_C(2) +
           (kind == PLAGUE_EVENT_END ? UINT64_C(1) : UINT64_C(0));
}

#endif
