#include "io/map_save_war_history.h"

#include "core/game_state.h"
#include "io/map_save.h"
#include "sim/diplomacy.h"
#include "sim/diplomacy_policy.h"
#include "sim/war_history.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char tag[8];
    int32_t version;
    int32_t item_size;
    int32_t count;
    int32_t aux_a;
    int32_t aux_b;
} WarHistoryBlockHeader;

typedef struct {
    int32_t uid;
    char name_en[NAME_LEN];
    char name_zh[NAME_LEN];
    uint32_t color;
} WarHistoryDiskPrincipal;

typedef struct {
    uint64_t war_serial;
    WarHistoryDiskPrincipal local;
    WarHistoryDiskPrincipal opponent;
    int32_t result;
    int32_t winner_uid;
    int32_t loser_uid;
    int32_t local_casualties;
    int32_t opponent_casualties;
    int32_t transferred_regions;
    int32_t indemnity_paid;
    int32_t beneficiary_uid;
    int32_t end_year;
    int32_t end_month;
    int32_t duration_months;
} WarHistoryDiskRecord;

typedef struct {
    int32_t owner_uid;
    int32_t count;
    uint64_t revision;
    WarHistoryDiskRecord records[WAR_HISTORY_CAPACITY];
} WarHistoryDiskCiv;

typedef struct {
    uint64_t next_serial;
    uint64_t global_revision;
    uint32_t checksum;
    uint32_t reserved;
    WarHistoryDiskCiv histories[MAX_CIVS];
} WarHistoryDiskPayload;

static const char WAR_HISTORY_TAG[8] = {'W', 'H', 'I', 'S', 'T', '2', '1', '\0'};

_Static_assert(sizeof(int) == 4, "MAP21 war-history integer contract changed");
_Static_assert(NAME_LEN == 64, "MAP21 war-history name contract changed");
_Static_assert(sizeof(WarHistoryBlockHeader) == 28,
               "MAP21 war-history block header changed");
_Static_assert(sizeof(WarHistoryDiskPrincipal) == 136,
               "MAP21 war-history principal changed");
_Static_assert(sizeof(WarHistoryDiskRecord) == 328,
               "MAP21 war-history record changed");
_Static_assert(sizeof(WarHistoryDiskCiv) == 1000,
               "MAP21 war-history civ payload changed");
_Static_assert(sizeof(WarHistoryDiskPayload) == 200024,
               "MAP21 war-history payload changed");

static uint32_t checksum_bytes(const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    uint32_t checksum = UINT32_C(2166136261);
    size_t i;
    for (i = 0; i < size; i++) {
        checksum ^= bytes[i];
        checksum *= UINT32_C(16777619);
    }
    return checksum;
}

static uint32_t payload_checksum(WarHistoryDiskPayload *payload) {
    uint32_t saved = payload->checksum;
    uint32_t checksum;
    payload->checksum = 0;
    checksum = checksum_bytes(payload, sizeof(*payload));
    payload->checksum = saved;
    return checksum;
}

static int bytes_are_zero(const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;
    for (i = 0; i < size; i++) if (bytes[i] != 0) return 0;
    return 1;
}

static int string_valid(const char text[NAME_LEN]) {
    return text[0] != '\0' && memchr(text, '\0', NAME_LEN) != NULL;
}

static int principal_valid(const WarHistoryPrincipal *principal) {
    return principal && principal->uid > WAR_HISTORY_INVALID_UID &&
           string_valid(principal->name_en) && string_valid(principal->name_zh) &&
           (principal->color & UINT32_C(0xff000000)) == 0;
}

static int result_participants_valid(const WarHistoryRecord *record) {
    int has_direction = diplomacy_policy_last_war_result_has_winner(record->result) ||
                        record->result == DIP_LAST_WAR_OFFENSIVE_HALTED;
    int local_uid = record->local.uid;
    int opponent_uid = record->opponent.uid;
    if (has_direction) {
        return record->winner_uid > WAR_HISTORY_INVALID_UID &&
               record->loser_uid > WAR_HISTORY_INVALID_UID &&
               record->winner_uid != record->loser_uid &&
               ((record->winner_uid == local_uid && record->loser_uid == opponent_uid) ||
                (record->winner_uid == opponent_uid && record->loser_uid == local_uid));
    }
    return record->winner_uid == WAR_HISTORY_INVALID_UID &&
           record->loser_uid == WAR_HISTORY_INVALID_UID;
}

static int settlement_valid(const WarHistoryRecord *record) {
    int has_settlement = record->transferred_regions > 0 || record->indemnity_paid > 0;
    if (!has_settlement) return record->beneficiary_uid == WAR_HISTORY_INVALID_UID;
    return diplomacy_policy_last_war_result_has_winner(record->result) &&
           record->beneficiary_uid == record->winner_uid &&
           (record->beneficiary_uid == record->local.uid ||
            record->beneficiary_uid == record->opponent.uid);
}

static int record_valid(const WarHistoryRecord *record, uint64_t next_serial,
                        int64_t save_absolute_month) {
    int64_t end_absolute_month;
    if (!record || record->war_serial == 0 || record->war_serial >= next_serial ||
        !principal_valid(&record->local) || !principal_valid(&record->opponent) ||
        record->local.uid == record->opponent.uid ||
        record->result == DIP_LAST_WAR_NONE ||
        !diplomacy_policy_last_war_result_valid(record->result) ||
        !result_participants_valid(record) ||
        record->local_casualties < 0 || record->opponent_casualties < 0 ||
        record->transferred_regions < 0 ||
        record->transferred_regions > MAX_NATURAL_REGIONS ||
        record->indemnity_paid < 0 || !settlement_valid(record) ||
        record->end_year < 0 || record->end_month < 1 || record->end_month > 12 ||
        record->duration_months < 1) return 0;
    end_absolute_month = (int64_t)record->end_year * 12 + record->end_month - 1;
    return end_absolute_month <= save_absolute_month &&
           record->duration_months <= end_absolute_month + 1;
}

static int principals_equal(const WarHistoryPrincipal *a,
                            const WarHistoryPrincipal *b) {
    return a->uid == b->uid && a->color == b->color &&
           memcmp(a->name_en, b->name_en, NAME_LEN) == 0 &&
           memcmp(a->name_zh, b->name_zh, NAME_LEN) == 0;
}

static int paired_records_equal(const WarHistoryRecord *a,
                                const WarHistoryRecord *b) {
    return a->local.uid != b->local.uid &&
           principals_equal(&a->local, &b->opponent) &&
           principals_equal(&a->opponent, &b->local) &&
           a->result == b->result && a->winner_uid == b->winner_uid &&
           a->loser_uid == b->loser_uid &&
           a->local_casualties == b->opponent_casualties &&
           a->opponent_casualties == b->local_casualties &&
           a->transferred_regions == b->transferred_regions &&
           a->indemnity_paid == b->indemnity_paid &&
           a->beneficiary_uid == b->beneficiary_uid &&
           a->end_year == b->end_year && a->end_month == b->end_month &&
           a->duration_months == b->duration_months;
}

static int duplicate_serials_valid(const WarHistorySaveState *state) {
    int civ_id;
    for (civ_id = 0; civ_id < MAX_CIVS; civ_id++) {
        const WarHistory *history = &state->histories[civ_id];
        int record_id;
        for (record_id = 0; record_id < history->count; record_id++) {
            const WarHistoryRecord *record = &history->records[record_id];
            int matches = 0;
            int other_civ;
            for (other_civ = 0; other_civ < MAX_CIVS; other_civ++) {
                const WarHistory *other_history = &state->histories[other_civ];
                int other_id;
                for (other_id = 0; other_id < other_history->count; other_id++) {
                    const WarHistoryRecord *other = &other_history->records[other_id];
                    if (record == other || record->war_serial != other->war_serial) continue;
                    matches++;
                    if (matches > 1 || !paired_records_equal(record, other)) return 0;
                }
            }
        }
    }
    return 1;
}

static int save_state_valid(const WarHistorySaveState *state, int save_year,
                            int save_month, int saved_civ_count) {
    int64_t save_absolute_month;
    int civ_id;
    if (!state || state->next_serial == 0 || state->global_revision == 0 ||
        save_year < 0 || save_month < 1 || save_month > 12 ||
        saved_civ_count < 0 || saved_civ_count > MAX_CIVS) return 0;
    save_absolute_month = (int64_t)save_year * 12 + save_month - 1;
    for (civ_id = 0; civ_id < MAX_CIVS; civ_id++) {
        const WarHistory *history = &state->histories[civ_id];
        uint64_t previous_serial = UINT64_MAX;
        int i;
        if (civ_id >= saved_civ_count) {
            if (!bytes_are_zero(history, sizeof(*history))) return 0;
            continue;
        }
        if (history->owner_uid <= WAR_HISTORY_INVALID_UID || history->count < 0 ||
            history->count > WAR_HISTORY_CAPACITY ||
            history->revision > state->global_revision ||
            history->revision == 0) return 0;
        for (i = 0; i < history->count; i++) {
            const WarHistoryRecord *record = &history->records[i];
            if (record->local.uid != history->owner_uid ||
                record->war_serial >= previous_serial ||
                !record_valid(record, state->next_serial, save_absolute_month)) return 0;
            previous_serial = record->war_serial;
        }
        for (; i < WAR_HISTORY_CAPACITY; i++) {
            if (!bytes_are_zero(&history->records[i], sizeof(history->records[i]))) return 0;
        }
    }
    return duplicate_serials_valid(state);
}

static void disk_principal_from_state(WarHistoryDiskPrincipal *dst,
                                      const WarHistoryPrincipal *src) {
    dst->uid = src->uid;
    memcpy(dst->name_en, src->name_en, NAME_LEN);
    memcpy(dst->name_zh, src->name_zh, NAME_LEN);
    dst->color = src->color;
}

static void state_principal_from_disk(WarHistoryPrincipal *dst,
                                      const WarHistoryDiskPrincipal *src) {
    dst->uid = src->uid;
    memcpy(dst->name_en, src->name_en, NAME_LEN);
    memcpy(dst->name_zh, src->name_zh, NAME_LEN);
    dst->color = src->color;
}

static void disk_record_from_state(WarHistoryDiskRecord *dst,
                                   const WarHistoryRecord *src) {
    dst->war_serial = src->war_serial;
    disk_principal_from_state(&dst->local, &src->local);
    disk_principal_from_state(&dst->opponent, &src->opponent);
    dst->result = src->result;
    dst->winner_uid = src->winner_uid;
    dst->loser_uid = src->loser_uid;
    dst->local_casualties = src->local_casualties;
    dst->opponent_casualties = src->opponent_casualties;
    dst->transferred_regions = src->transferred_regions;
    dst->indemnity_paid = src->indemnity_paid;
    dst->beneficiary_uid = src->beneficiary_uid;
    dst->end_year = src->end_year;
    dst->end_month = src->end_month;
    dst->duration_months = src->duration_months;
}

static void state_record_from_disk(WarHistoryRecord *dst,
                                   const WarHistoryDiskRecord *src) {
    dst->war_serial = src->war_serial;
    state_principal_from_disk(&dst->local, &src->local);
    state_principal_from_disk(&dst->opponent, &src->opponent);
    dst->result = src->result;
    dst->winner_uid = src->winner_uid;
    dst->loser_uid = src->loser_uid;
    dst->local_casualties = src->local_casualties;
    dst->opponent_casualties = src->opponent_casualties;
    dst->transferred_regions = src->transferred_regions;
    dst->indemnity_paid = src->indemnity_paid;
    dst->beneficiary_uid = src->beneficiary_uid;
    dst->end_year = src->end_year;
    dst->end_month = src->end_month;
    dst->duration_months = src->duration_months;
}

static void payload_from_state(WarHistoryDiskPayload *payload,
                               const WarHistorySaveState *state) {
    int civ_id;
    payload->next_serial = state->next_serial;
    payload->global_revision = state->global_revision;
    for (civ_id = 0; civ_id < MAX_CIVS; civ_id++) {
        const WarHistory *src = &state->histories[civ_id];
        WarHistoryDiskCiv *dst = &payload->histories[civ_id];
        int i;
        dst->owner_uid = src->owner_uid;
        dst->count = src->count;
        dst->revision = src->revision;
        for (i = 0; i < WAR_HISTORY_CAPACITY; i++) {
            disk_record_from_state(&dst->records[i], &src->records[i]);
        }
    }
    payload->checksum = payload_checksum(payload);
}

static void state_from_payload(WarHistorySaveState *state,
                               const WarHistoryDiskPayload *payload) {
    int civ_id;
    state->next_serial = payload->next_serial;
    state->global_revision = payload->global_revision;
    for (civ_id = 0; civ_id < MAX_CIVS; civ_id++) {
        const WarHistoryDiskCiv *src = &payload->histories[civ_id];
        WarHistory *dst = &state->histories[civ_id];
        int i;
        dst->owner_uid = src->owner_uid;
        dst->count = src->count;
        dst->revision = src->revision;
        for (i = 0; i < WAR_HISTORY_CAPACITY; i++) {
            state_record_from_disk(&dst->records[i], &src->records[i]);
        }
    }
}

static int header_valid(const WarHistoryBlockHeader *header) {
    return header && memcmp(header->tag, WAR_HISTORY_TAG, sizeof(header->tag)) == 0 &&
           header->version == MAP_SAVE_WAR_HISTORY_BLOCK_VERSION &&
           header->item_size == (int32_t)sizeof(WarHistoryDiskPayload) &&
           header->count == 1 && header->aux_a == MAX_CIVS &&
           header->aux_b == WAR_HISTORY_CAPACITY;
}

int map_save_war_history_write(FILE *file) {
    WarHistorySaveState *state;
    WarHistoryDiskPayload *payload;
    WarHistoryBlockHeader header;
    int ok = 0;
    if (!file) return 0;
    state = (WarHistorySaveState *)calloc(1, sizeof(*state));
    payload = (WarHistoryDiskPayload *)calloc(1, sizeof(*payload));
    if (!state || !payload) goto done;
    war_history_copy_save_state(state);
    if (!save_state_valid(state, year, month, civ_count)) goto done;
    payload_from_state(payload, state);
    memset(&header, 0, sizeof(header));
    memcpy(header.tag, WAR_HISTORY_TAG, sizeof(header.tag));
    header.version = MAP_SAVE_WAR_HISTORY_BLOCK_VERSION;
    header.item_size = (int32_t)sizeof(*payload);
    header.count = 1;
    header.aux_a = MAX_CIVS;
    header.aux_b = WAR_HISTORY_CAPACITY;
    ok = fwrite(&header, sizeof(header), 1, file) == 1 &&
         fwrite(payload, sizeof(*payload), 1, file) == 1;
done:
    free(payload);
    free(state);
    return ok;
}

int map_save_war_history_stage_read(FILE *file, int save_version,
                                    int save_year, int save_month,
                                    int saved_civ_count,
                                    MapSaveWarHistoryStage *stage) {
    WarHistoryBlockHeader header;
    WarHistoryDiskPayload *payload = NULL;
    WarHistorySaveState *candidate = NULL;
    uint32_t stored_checksum;
    int ok = 0;
    if (!file || !stage || stage->state || stage->ready ||
        !map_save_version_supported(save_version)) return 0;
    if (fread(&header, sizeof(header), 1, file) != 1 || !header_valid(&header)) return 0;
    payload = (WarHistoryDiskPayload *)malloc(sizeof(*payload));
    candidate = (WarHistorySaveState *)calloc(1, sizeof(*candidate));
    if (!payload || !candidate || fread(payload, sizeof(*payload), 1, file) != 1) goto done;
    stored_checksum = payload->checksum;
    if (payload->reserved != 0 || stored_checksum == 0 ||
        payload_checksum(payload) != stored_checksum) goto done;
    state_from_payload(candidate, payload);
    if (!save_state_valid(candidate, save_year, save_month, saved_civ_count)) goto done;
    stage->state = candidate;
    stage->ready = 1;
    candidate = NULL;
    ok = 1;
done:
    free(candidate);
    free(payload);
    return ok;
}

const WarHistorySaveState *map_save_war_history_stage_state(
    const MapSaveWarHistoryStage *stage) {
    return stage && stage->ready ? stage->state : NULL;
}

static int loaded_uids_match(const WarHistorySaveState *state) {
    int civ_id;
    if (!state || civ_count < 0 || civ_count > MAX_CIVS) return 0;
    for (civ_id = 0; civ_id < MAX_CIVS; civ_id++) {
        const WarHistory *history = &state->histories[civ_id];
        if (civ_id >= civ_count) {
            if (!bytes_are_zero(history, sizeof(*history))) return 0;
        } else if (history->owner_uid != civs[civ_id].uid) {
            return 0;
        }
    }
    return 1;
}

int map_save_war_history_stage_commit(const MapSaveWarHistoryStage *stage) {
    const WarHistorySaveState *state = map_save_war_history_stage_state(stage);
    return state && loaded_uids_match(state) && war_history_restore_save_state(state);
}

void map_save_war_history_stage_release(MapSaveWarHistoryStage *stage) {
    if (!stage) return;
    if (stage->state) memset(stage->state, 0, sizeof(*stage->state));
    free(stage->state);
    stage->state = NULL;
    stage->ready = 0;
}
