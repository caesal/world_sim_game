#include "io/map_save_state.h"

#include "core/game_types.h"
#include "sim/diplomacy.h"
#include "io/map_save_legacy.h"
#include "sim/plague.h"
#include "sim/war.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    char tag[8];
    int version;
    int item_size;
    int count;
    int aux_a;
    int aux_b;
} SaveBlockHeader;

static DiplomacyRelation save_relations[MAX_CIVS * MAX_CIVS];
static ActiveWar save_wars[WAR_SAVE_SLOT_COUNT];
static PlagueState save_plague_cities[MAX_CITIES];
static EventLogEntry save_events[EVENT_LOG_COUNT];
static int save_support[MAX_CIVS];
static int save_route_exposure[MAX_MARITIME_ROUTES];

static int write_all(FILE *file, const void *data, size_t size, size_t count) {
    return fwrite(data, size, count, file) == count;
}

static int read_all(FILE *file, void *data, size_t size, size_t count) {
    return fread(data, size, count, file) == count;
}

static int write_block(FILE *file, const char *tag, const void *data,
                       int item_size, int count, int aux_a, int aux_b) {
    SaveBlockHeader header;
    memset(&header, 0, sizeof(header));
    snprintf(header.tag, sizeof(header.tag), "%s", tag);
    header.version = 1; header.item_size = item_size; header.count = count;
    header.aux_a = aux_a; header.aux_b = aux_b;
    return write_all(file, &header, sizeof(header), 1) &&
           (count <= 0 || write_all(file, data, (size_t)item_size, (size_t)count));
}

static int read_header(FILE *file, SaveBlockHeader *header, const char *tag, int item_size, int max_count) {
    return read_all(file, header, sizeof(*header), 1) &&
           strncmp(header->tag, tag, sizeof(header->tag)) == 0 &&
           header->version == 1 && header->item_size == item_size &&
           header->count >= 0 && header->count <= max_count;
}

static int read_header_any_size(FILE *file, SaveBlockHeader *header, const char *tag, int max_count) {
    return read_all(file, header, sizeof(*header), 1) &&
           strncmp(header->tag, tag, sizeof(header->tag)) == 0 &&
           header->version == 1 && header->count >= 0 && header->count <= max_count;
}

static int read_plague_city_state(FILE *file, int save_version, int *last_plague_city) {
    SaveBlockHeader header;
    int i;

    if (!read_header_any_size(file, &header, "PLGC", MAX_CITIES)) return 0;
    memset(save_plague_cities, 0, sizeof(save_plague_cities));
    if (last_plague_city) *last_plague_city = header.aux_a;
    if (header.item_size == sizeof(PlagueState)) {
        return read_all(file, save_plague_cities, sizeof(PlagueState), (size_t)header.count);
    }
    if (save_version <= 10 && header.item_size == sizeof(LegacyPlagueStateV10)) {
        LegacyPlagueStateV10 legacy[MAX_CITIES];
        if (!read_all(file, legacy, sizeof(LegacyPlagueStateV10), (size_t)header.count)) return 0;
        for (i = 0; i < header.count; i++) {
            save_plague_cities[i].active = legacy[i].active;
            save_plague_cities[i].infected = legacy[i].infected;
            save_plague_cities[i].severity = legacy[i].severity;
            save_plague_cities[i].months_left = legacy[i].months_left;
            save_plague_cities[i].immunity = legacy[i].immunity;
            save_plague_cities[i].deaths_total = legacy[i].deaths_total;
            save_plague_cities[i].origin_city = legacy[i].origin_city;
            save_plague_cities[i].age_months = legacy[i].age_months;
            save_plague_cities[i].reinfection_cooldown_months = 0;
        }
        return 1;
    }
    return 0;
}

static int read_war_state(FILE *file, int save_version, int *total_started) {
    SaveBlockHeader header;
    size_t old_size = offsetof(ActiveWar, temporary_soldiers_a);
    int i;
    if (!read_header_any_size(file, &header, "WAR", WAR_SAVE_SLOT_COUNT)) return 0;
    memset(save_wars, 0, sizeof(save_wars));
    if (total_started) *total_started = header.aux_a;
    if (header.item_size == sizeof(ActiveWar)) {
        return read_all(file, save_wars, sizeof(ActiveWar), (size_t)header.count);
    }
    if (save_version <= 11 && header.item_size == (int)old_size) {
        for (i = 0; i < header.count; i++) {
            if (!read_all(file, &save_wars[i], old_size, 1)) return 0;
        }
        return 1;
    }
    return 0;
}

int map_save_write_dynamic_state(FILE *file) {
    int total_started = 0, last_plague_city = -1, event_count = 0, event_next = 0, event_total = 0;
    int a, b;

    for (a = 0; a < MAX_CIVS; a++) for (b = 0; b < MAX_CIVS; b++) save_relations[a * MAX_CIVS + b] = diplomacy_relation(a, b);
    memset(save_wars, 0, sizeof(save_wars)); memset(save_support, 0, sizeof(save_support));
    war_copy_save_state(save_wars, WAR_SAVE_SLOT_COUNT, save_support, MAX_CIVS, &total_started);
    plague_copy_save_state(save_plague_cities, MAX_CITIES, save_route_exposure, MAX_MARITIME_ROUTES, &last_plague_city);
    memset(save_events, 0, sizeof(save_events)); event_log_copy_save_state(save_events, EVENT_LOG_COUNT, &event_count, &event_next, &event_total);
    return write_block(file, "DIPLO", save_relations, sizeof(DiplomacyRelation), MAX_CIVS * MAX_CIVS, 0, 0) &&
           write_block(file, "WAR", save_wars, sizeof(ActiveWar), WAR_SAVE_SLOT_COUNT, total_started, 0) &&
           write_block(file, "WSUP", save_support, sizeof(int), MAX_CIVS, 0, 0) &&
           write_block(file, "PLGC", save_plague_cities, sizeof(PlagueState), MAX_CITIES, last_plague_city, 0) &&
           write_block(file, "PLGR", save_route_exposure, sizeof(int), MAX_MARITIME_ROUTES, 0, 0) &&
           write_block(file, "ELOG", save_events, sizeof(EventLogEntry), EVENT_LOG_COUNT, event_count, event_next) &&
           write_block(file, "ELOT", &event_total, sizeof(int), 1, 0, 0);
}

int map_save_read_dynamic_state(FILE *file, int save_version) {
    SaveBlockHeader header;
    int event_total = 0;
    int a, b, total_started, last_plague_city, event_count, event_next;

    if (save_version < 8) return 0;
    if (!read_header(file, &header, "DIPLO", sizeof(DiplomacyRelation), MAX_CIVS * MAX_CIVS) ||
        !read_all(file, save_relations, sizeof(DiplomacyRelation), (size_t)header.count)) return -1;
    diplomacy_reset();
    for (a = 0; a < MAX_CIVS; a++) for (b = 0; b < MAX_CIVS; b++) diplomacy_restore_relation(a, b, save_relations[a * MAX_CIVS + b]);
    if (!read_war_state(file, save_version, &total_started)) return -1;
    if (!read_header(file, &header, "WSUP", sizeof(int), MAX_CIVS) || !read_all(file, save_support, sizeof(int), (size_t)header.count)) return -1;
    if (!read_plague_city_state(file, save_version, &last_plague_city)) return -1;
    if (!read_header(file, &header, "PLGR", sizeof(int), MAX_MARITIME_ROUTES) ||
        !read_all(file, save_route_exposure, sizeof(int), (size_t)header.count)) return -1;
    if (!read_header(file, &header, "ELOG", sizeof(EventLogEntry), EVENT_LOG_COUNT) ||
        !read_all(file, save_events, sizeof(EventLogEntry), (size_t)header.count)) return -1;
    event_count = header.aux_a; event_next = header.aux_b;
    if (!read_header(file, &header, "ELOT", sizeof(int), 1) || !read_all(file, &event_total, sizeof(int), 1)) return -1;
    diplomacy_sanitize_loaded(); war_restore_save_state(save_wars, WAR_SAVE_SLOT_COUNT, save_support, MAX_CIVS, total_started);
    plague_restore_save_state(save_plague_cities, MAX_CITIES, save_route_exposure, MAX_MARITIME_ROUTES, last_plague_city);
    event_log_restore_save_state(save_events, event_count, event_next, event_total);
    return 1;
}
