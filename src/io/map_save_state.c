#include "io/map_save_state.h"

#include "core/game_types.h"
#include "io/map_save.h"
#include "io/map_save_legacy.h"
#include "io/map_save_plague.h"
#include "sim/diplomacy.h"
#include "sim/alliance.h"
#include "sim/war.h"

#include <stddef.h>
#include <stdlib.h>
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
static AllianceSaveState save_alliances;
static EventLogEntry save_events[EVENT_LOG_COUNT];
static int save_support[MAX_CIVS];

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

static int read_war_state(FILE *file, int save_version, int *total_started) {
    SaveBlockHeader header;
    size_t old_size = offsetof(ActiveWar, temporary_soldiers_a);
    int i;
    if (!read_header_any_size(file, &header, "WAR", WAR_SAVE_SLOT_COUNT)) return 0;
    memset(save_wars, 0, sizeof(save_wars));
    if (total_started) *total_started = header.aux_a;
    if (save_version >= 21 &&
        (header.item_size != (int)sizeof(ActiveWar) ||
         header.count != WAR_SAVE_SLOT_COUNT || header.aux_a < 0 || header.aux_b != 0)) {
        return 0;
    }
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

static int compare_serials(const void *left, const void *right) {
    uint64_t a = *(const uint64_t *)left;
    uint64_t b = *(const uint64_t *)right;
    return a < b ? -1 : a > b ? 1 : 0;
}

static int block_is_zero(const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;
    for (i = 0; i < size; i++) if (bytes[i] != 0) return 0;
    return 1;
}

static int active_wars_valid(const WarHistorySaveState *history_state) {
    uint64_t *serials;
    int64_t current_absolute_month;
    int serial_count = 0;
    int i;
    if (!history_state || history_state->next_serial == 0 ||
        year < 0 || month < 1 || month > 12) return 0;
    current_absolute_month = (int64_t)year * 12 + month - 1;
    serials = (uint64_t *)malloc(sizeof(*serials) * WAR_SAVE_SLOT_COUNT);
    if (!serials) return 0;
    for (i = 0; i < WAR_SAVE_SLOT_COUNT; i++) {
        const ActiveWar *war = &save_wars[i];
        if (war->active == 0) {
            if (!block_is_zero(war, sizeof(*war))) goto invalid;
            continue;
        }
        if (war->active != 1 || war->attacker < 0 || war->attacker >= civ_count ||
            war->defender < 0 || war->defender >= civ_count ||
            war->attacker == war->defender || !civs[war->attacker].alive ||
            !civs[war->defender].alive || war->war_serial == 0 ||
            war->war_serial >= history_state->next_serial ||
            war->attacker_uid <= WAR_HISTORY_INVALID_UID ||
            war->defender_uid <= WAR_HISTORY_INVALID_UID ||
            war->attacker_uid != civs[war->attacker].uid ||
            war->defender_uid != civs[war->defender].uid ||
            war->start_absolute_month < 0 ||
            war->start_absolute_month > current_absolute_month) goto invalid;
        serials[serial_count++] = war->war_serial;
    }
    qsort(serials, (size_t)serial_count, sizeof(*serials), compare_serials);
    for (i = 1; i < serial_count; i++) {
        if (serials[i - 1] == serials[i]) goto invalid;
    }
    for (i = 0; i < MAX_CIVS; i++) {
        const WarHistory *history = &history_state->histories[i];
        int record_id;
        for (record_id = 0; record_id < history->count; record_id++) {
            uint64_t serial = history->records[record_id].war_serial;
            if (bsearch(&serial, serials, (size_t)serial_count,
                        sizeof(*serials), compare_serials)) goto invalid;
        }
    }
    free(serials);
    return 1;
invalid:
    free(serials);
    return 0;
}

static int read_alliance_state(FILE *file, int save_version) {
    SaveBlockHeader header;
    size_t v14_size = offsetof(AllianceSaveState, candidate_count);
    size_t v15_size = offsetof(AllianceSaveState, alliance_type);
    size_t v16_size = offsetof(AllianceSaveState, vote_council_valid);
    size_t v17_size = offsetof(AllianceSaveState, council_previous_valid);
    if (save_version < 14) {
        alliance_reset();
        return 1;
    }
    if (!read_header_any_size(file, &header, "ALLY", 1)) return 0;
    memset(&save_alliances, 0, sizeof(save_alliances));
    if (header.item_size == sizeof(AllianceSaveState)) {
        if (!read_all(file, &save_alliances, sizeof(AllianceSaveState), 1)) return 0;
    } else if (save_version == 14 && header.item_size == (int)v14_size) {
        if (!read_all(file, &save_alliances, v14_size, 1)) return 0;
    } else if (save_version <= 15 && header.item_size == (int)v15_size) {
        if (!read_all(file, &save_alliances, v15_size, 1)) return 0;
    } else if (save_version <= 16 && header.item_size == (int)v16_size) {
        if (!read_all(file, &save_alliances, v16_size, 1)) return 0;
    } else if (save_version <= 17 && header.item_size == (int)v17_size) {
        if (!read_all(file, &save_alliances, v17_size, 1)) return 0;
    } else {
        return 0;
    }
    alliance_restore_save_state(&save_alliances);
    return 1;
}

int map_save_write_dynamic_state(FILE *file) {
    int total_started = 0, event_count = 0, event_next = 0, event_total = 0;
    int a, b;

    for (a = 0; a < MAX_CIVS; a++) for (b = 0; b < MAX_CIVS; b++) save_relations[a * MAX_CIVS + b] = diplomacy_relation(a, b);
    memset(save_wars, 0, sizeof(save_wars)); memset(save_support, 0, sizeof(save_support));
    war_copy_save_state(save_wars, WAR_SAVE_SLOT_COUNT, save_support, MAX_CIVS, &total_started);
    alliance_copy_save_state(&save_alliances);
    memset(save_events, 0, sizeof(save_events)); event_log_copy_save_state(save_events, EVENT_LOG_COUNT, &event_count, &event_next, &event_total);
    return write_block(file, "DIPLO", save_relations, sizeof(DiplomacyRelation), MAX_CIVS * MAX_CIVS, 0, 0) &&
           write_block(file, "ALLY", &save_alliances, sizeof(AllianceSaveState), 1, 0, 0) &&
           write_block(file, "WAR", save_wars, sizeof(ActiveWar), WAR_SAVE_SLOT_COUNT, total_started, 0) &&
           write_block(file, "WSUP", save_support, sizeof(int), MAX_CIVS, 0, 0) &&
           map_save_plague_write(file) &&
           write_block(file, "ELOG", save_events, sizeof(EventLogEntry), EVENT_LOG_COUNT, event_count, event_next) &&
           write_block(file, "ELOT", &event_total, sizeof(int), 1, 0, 0);
}

int map_save_read_dynamic_state_with_history(
    FILE *file, int save_version, const WarHistorySaveState *history_state) {
    SaveBlockHeader header;
    int event_total = 0;
    int a, b, total_started, event_count, event_next;

    if (!map_save_version_supported(save_version)) return -1;
    if (!read_header(file, &header, "DIPLO", sizeof(DiplomacyRelation), MAX_CIVS * MAX_CIVS) ||
        !read_all(file, save_relations, sizeof(DiplomacyRelation), (size_t)header.count)) return -1;
    diplomacy_reset();
    for (a = 0; a < MAX_CIVS; a++) for (b = 0; b < MAX_CIVS; b++) diplomacy_restore_relation(a, b, save_relations[a * MAX_CIVS + b]);
    if (!read_alliance_state(file, save_version)) return -1;
    if (!read_war_state(file, save_version, &total_started)) return -1;
    if (!read_header(file, &header, "WSUP", sizeof(int), MAX_CIVS) ||
        header.count != MAX_CIVS || header.aux_a != 0 || header.aux_b != 0 ||
        !read_all(file, save_support, sizeof(int), (size_t)header.count)) return -1;
    if (history_state && !active_wars_valid(history_state)) return -1;
    if (!map_save_plague_read(file)) return -1;
    if (!read_header(file, &header, "ELOG", sizeof(EventLogEntry), EVENT_LOG_COUNT) ||
        !read_all(file, save_events, sizeof(EventLogEntry), (size_t)header.count)) return -1;
    event_count = header.aux_a; event_next = header.aux_b;
    if (!read_header(file, &header, "ELOT", sizeof(int), 1) || !read_all(file, &event_total, sizeof(int), 1)) return -1;
    diplomacy_sanitize_loaded(); alliance_sanitize_loaded();
    war_restore_save_state(save_wars, WAR_SAVE_SLOT_COUNT, save_support, MAX_CIVS, total_started);
    event_log_restore_save_state(save_events, event_count, event_next, event_total);
    return 1;
}

int map_save_read_dynamic_state(FILE *file, int save_version) {
    return map_save_read_dynamic_state_with_history(file, save_version, NULL);
}
