#include "game/game_war_history_save_probe_internal.h"

#include "core/game_types.h"
#include "io/map_save.h"
#include "io/map_save_war_history.h"
#include "sim/war_history.h"

#include <limits.h>
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
} ProbeHistoryHeader;

typedef struct {
    int32_t uid;
    char name_en[NAME_LEN];
    char name_zh[NAME_LEN];
    uint32_t color;
} ProbeHistoryPrincipal;

typedef struct {
    uint64_t war_serial;
    ProbeHistoryPrincipal local;
    ProbeHistoryPrincipal opponent;
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
} ProbeHistoryRecord;

typedef struct {
    int32_t owner_uid;
    int32_t count;
    uint64_t revision;
    ProbeHistoryRecord records[WAR_HISTORY_CAPACITY];
} ProbeHistoryCiv;

typedef struct {
    uint64_t next_serial;
    uint64_t global_revision;
    uint32_t checksum;
    uint32_t reserved;
    ProbeHistoryCiv histories[MAX_CIVS];
} ProbeHistoryPayload;

typedef enum {
    MUTATE_CHECKSUM,
    MUTATE_COUNT,
    MUTATE_RESULT,
    MUTATE_DATE,
    MUTATE_MONTH,
    MUTATE_UID,
    MUTATE_STRING,
    MUTATE_COLOR,
    MUTATE_SETTLEMENT,
    MUTATE_DIMENSIONS,
    MUTATE_OVERSIZE,
    MUTATE_MIRROR,
    MUTATE_NEXT_SERIAL,
    MUTATE_RESERVED,
    MUTATE_DURATION_ZERO,
    MUTATE_DURATION_EXCESSIVE,
    MUTATE_NEGATIVE_CASUALTIES,
    MUTATE_NEGATIVE_CESSION,
    MUTATE_OVERSIZED_CESSION,
    MUTATE_NEGATIVE_INDEMNITY,
    MUTATE_SERIAL_ORDER,
    MUTATE_SERIAL_DUPLICATE,
    MUTATE_UNUSED_RECORD,
    MUTATE_ZERO_COUNT_RECORD,
    MUTATE_ZERO_COUNT_OWNER_UID,
    MUTATE_OWNER_LOCAL_UID
} CodecMutation;

typedef struct {
    const char *name;
    CodecMutation mutation;
} MutationCase;

_Static_assert(sizeof(ProbeHistoryHeader) == 28, "probe WHIST21 header mismatch");
_Static_assert(sizeof(ProbeHistoryRecord) == 328, "probe WHIST21 record mismatch");
_Static_assert(sizeof(ProbeHistoryCiv) == 1000, "probe WHIST21 civ mismatch");
_Static_assert(sizeof(ProbeHistoryPayload) == 200024, "probe WHIST21 payload mismatch");

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

static void repair_checksum(ProbeHistoryPayload *payload) {
    payload->checksum = 0;
    payload->checksum = checksum_bytes(payload, sizeof(*payload));
}

static int apply_mutation(unsigned char *bytes, CodecMutation mutation) {
    ProbeHistoryHeader *header = (ProbeHistoryHeader *)bytes;
    ProbeHistoryPayload *payload = (ProbeHistoryPayload *)malloc(sizeof(*payload));
    ProbeHistoryRecord *record;
    int payload_changed = 1;
    if (!payload) return 0;
    memcpy(payload, bytes + sizeof(*header), sizeof(*payload));
    record = &payload->histories[0].records[0];
    switch (mutation) {
        case MUTATE_CHECKSUM: payload->checksum ^= UINT32_C(0x01010101); payload_changed = 0; break;
        case MUTATE_COUNT: payload->histories[0].count = WAR_HISTORY_CAPACITY + 1; break;
        case MUTATE_RESULT: record->result = 999; break;
        case MUTATE_DATE: record->end_year = 13; break;
        case MUTATE_MONTH: record->end_month = 13; break;
        case MUTATE_UID: record->local.uid = WAR_HISTORY_INVALID_UID; break;
        case MUTATE_STRING: memset(record->local.name_en, 'X', NAME_LEN); break;
        case MUTATE_COLOR: record->local.color |= UINT32_C(0xff000000); break;
        case MUTATE_SETTLEMENT: record->beneficiary_uid = record->opponent.uid; break;
        case MUTATE_DIMENSIONS: header->aux_a = MAX_CIVS - 1; payload_changed = 0; break;
        case MUTATE_OVERSIZE: header->item_size = INT_MAX; payload_changed = 0; break;
        case MUTATE_MIRROR:
            payload->histories[1].records[0].local_casualties++;
            break;
        case MUTATE_NEXT_SERIAL: payload->next_serial = 0; break;
        case MUTATE_RESERVED: payload->reserved = 1; break;
        case MUTATE_DURATION_ZERO: record->duration_months = 0; break;
        case MUTATE_DURATION_EXCESSIVE: record->duration_months = 148; break;
        case MUTATE_NEGATIVE_CASUALTIES: record->local_casualties = -1; break;
        case MUTATE_NEGATIVE_CESSION: record->transferred_regions = -1; break;
        case MUTATE_OVERSIZED_CESSION:
            record->transferred_regions = MAX_NATURAL_REGIONS + 1;
            break;
        case MUTATE_NEGATIVE_INDEMNITY: record->indemnity_paid = -1; break;
        case MUTATE_SERIAL_ORDER:
            payload->histories[0].records[0].war_serial = 70;
            break;
        case MUTATE_SERIAL_DUPLICATE:
            payload->histories[0].records[1].war_serial = record->war_serial;
            break;
        case MUTATE_UNUSED_RECORD:
            payload->histories[1].records[1].war_serial = 1;
            break;
        case MUTATE_ZERO_COUNT_RECORD:
            payload->histories[3].records[0].war_serial = 1;
            break;
        case MUTATE_ZERO_COUNT_OWNER_UID:
            payload->histories[3].owner_uid = WAR_HISTORY_INVALID_UID;
            break;
        case MUTATE_OWNER_LOCAL_UID:
            payload->histories[0].owner_uid = record->opponent.uid;
            break;
    }
    if (payload_changed && mutation != MUTATE_CHECKSUM) repair_checksum(payload);
    memcpy(bytes + sizeof(*header), payload, sizeof(*payload));
    free(payload);
    return 1;
}

static int live_state_unchanged(const WarHistorySaveState *before) {
    WarHistorySaveState *after = (WarHistorySaveState *)calloc(1, sizeof(*after));
    int equal = 0;
    if (after) {
        war_history_copy_save_state(after);
        equal = war_history_save_probe_state_equal(before, after);
    }
    free(after);
    return equal;
}

static void check_rejected(WarHistorySaveProbeContext *context,
                           const char *name, const unsigned char *bytes,
                           size_t size, int save_version,
                           const WarHistorySaveState *live_before) {
    MapSaveWarHistoryStage stage = {0};
    FILE *file = war_history_save_probe_file_from_bytes(bytes, size);
    int accepted = file ? map_save_war_history_stage_read(
        file, save_version, year, month, civ_count, &stage) : 0;
    int unchanged = live_state_unchanged(live_before);
    long position = file ? ftell(file) : -1;
    int position_ok = save_version != 20 || position == 0;
    war_history_save_probe_check(context, name,
        file && !accepted && !stage.ready && unchanged && position_ok,
        "accepted=%d ready=%d nonmutation=%d position=%ld bytes=%u",
        accepted, stage.ready, unchanged, position, (unsigned)size);
    map_save_war_history_stage_release(&stage);
    if (file) fclose(file);
}

static int write_baseline(WarHistorySaveState *expected,
                          unsigned char **bytes, size_t *size) {
    FILE *file;
    if (!war_history_restore_save_state(expected)) return 0;
    file = tmpfile();
    if (!file) return 0;
    if (!map_save_war_history_write(file) ||
        !war_history_save_probe_read_file(file, bytes, size)) {
        fclose(file);
        return 0;
    }
    fclose(file);
    return 1;
}

static void check_roundtrip_and_commit(WarHistorySaveProbeContext *context,
                                       const unsigned char *bytes, size_t size,
                                       const WarHistorySaveState *expected) {
    MapSaveWarHistoryStage stage = {0};
    WarHistorySaveState *actual = (WarHistorySaveState *)calloc(1, sizeof(*actual));
    WarHistorySaveState *stage_before = (WarHistorySaveState *)calloc(1, sizeof(*stage_before));
    WarHistorySaveState *stage_after = (WarHistorySaveState *)calloc(1, sizeof(*stage_after));
    FILE *file = war_history_save_probe_file_from_bytes(bytes, size);
    int read_ok;
    int stage_nonmutation;
    if (stage_before) {
        memcpy(stage_before, expected, sizeof(*stage_before));
        stage_before->next_serial = 101;
        stage_before->global_revision = 600;
        war_history_restore_save_state(stage_before);
    }
    read_ok = file && map_save_war_history_stage_read(
        file, map_save_current_version(), year, month, civ_count, &stage);
    if (stage_after) war_history_copy_save_state(stage_after);
    stage_nonmutation = stage_before && stage_after &&
        war_history_save_probe_state_equal(stage_before, stage_after);
    int staged_equal = read_ok && war_history_save_probe_state_equal(
        expected, map_save_war_history_stage_state(&stage));
    int commit_ok = read_ok && map_save_war_history_stage_commit(&stage);
    if (actual) war_history_copy_save_state(actual);
    war_history_save_probe_check(context, "exact_state_revision_next_serial_roundtrip",
        actual && staged_equal && commit_ok &&
        war_history_save_probe_state_equal(expected, actual),
        "read=%d staged_equal=%d commit=%d next=%llu revision=%llu",
        read_ok, staged_equal, commit_ok,
        (unsigned long long)(actual ? actual->next_serial : 0),
        (unsigned long long)(actual ? actual->global_revision : 0));
    war_history_save_probe_check(context, "successful_stage_is_nonmutating",
        read_ok && stage_nonmutation,
        "read=%d nonmutation=%d", read_ok, stage_nonmutation);
    if (read_ok && actual) {
        WarHistorySaveState *before = (WarHistorySaveState *)calloc(1, sizeof(*before));
        int mismatch_commit;
        int unchanged;
        war_history_copy_save_state(before);
        civs[1].uid++;
        mismatch_commit = map_save_war_history_stage_commit(&stage);
        unchanged = live_state_unchanged(before);
        war_history_save_probe_check(context, "stage_commit_uid_mismatch_nonmutation",
            !mismatch_commit && unchanged,
            "commit=%d nonmutation=%d", mismatch_commit, unchanged);
        civs[1].uid--;
        free(before);
    }
    map_save_war_history_stage_release(&stage);
    free(stage_after);
    free(stage_before);
    free(actual);
    if (file) fclose(file);
}

int game_war_history_save_probe_codec(WarHistorySaveProbeContext *context) {
    static const MutationCase cases[] = {
        {"payload_checksum_corruption_rejected", MUTATE_CHECKSUM},
        {"invalid_history_count_rejected", MUTATE_COUNT},
        {"invalid_result_enum_rejected", MUTATE_RESULT},
        {"future_end_date_rejected", MUTATE_DATE},
        {"invalid_month_rejected", MUTATE_MONTH},
        {"invalid_principal_uid_rejected", MUTATE_UID},
        {"unterminated_name_rejected", MUTATE_STRING},
        {"invalid_color_rejected", MUTATE_COLOR},
        {"invalid_settlement_beneficiary_rejected", MUTATE_SETTLEMENT},
        {"invalid_payload_dimensions_rejected", MUTATE_DIMENSIONS},
        {"oversized_payload_rejected", MUTATE_OVERSIZE},
        {"malformed_mirrored_pair_rejected", MUTATE_MIRROR},
        {"invalid_next_serial_rejected", MUTATE_NEXT_SERIAL},
        {"nonzero_reserved_payload_rejected", MUTATE_RESERVED},
        {"zero_duration_rejected", MUTATE_DURATION_ZERO},
        {"excessive_duration_rejected", MUTATE_DURATION_EXCESSIVE},
        {"negative_casualties_rejected", MUTATE_NEGATIVE_CASUALTIES},
        {"negative_cession_rejected", MUTATE_NEGATIVE_CESSION},
        {"oversized_cession_rejected", MUTATE_OVERSIZED_CESSION},
        {"negative_indemnity_rejected", MUTATE_NEGATIVE_INDEMNITY},
        {"non_descending_serial_order_rejected", MUTATE_SERIAL_ORDER},
        {"duplicate_history_serial_rejected", MUTATE_SERIAL_DUPLICATE},
        {"nonzero_unused_record_rejected", MUTATE_UNUSED_RECORD},
        {"zero_count_nonzero_record_rejected", MUTATE_ZERO_COUNT_RECORD},
        {"zero_count_invalid_owner_uid_rejected", MUTATE_ZERO_COUNT_OWNER_UID},
        {"owner_local_uid_disagreement_rejected", MUTATE_OWNER_LOCAL_UID}
    };
    WarHistorySaveState *expected = (WarHistorySaveState *)calloc(1, sizeof(*expected));
    unsigned char *baseline = NULL;
    unsigned char *scratch = NULL;
    size_t size = 0;
    size_t i;
    int start_failures = context->failures;
    if (!expected) return 0;
    war_history_save_probe_make_state(expected);
    if (!write_baseline(expected, &baseline, &size)) {
        war_history_save_probe_check(context, "write_valid_whist21", 0,
                                     "baseline write failed");
        free(expected);
        return 0;
    }
    war_history_save_probe_check(context, "write_valid_whist21",
        size == sizeof(ProbeHistoryHeader) + sizeof(ProbeHistoryPayload),
        "bytes=%u expected=%u", (unsigned)size,
        (unsigned)(sizeof(ProbeHistoryHeader) + sizeof(ProbeHistoryPayload)));
    check_roundtrip_and_commit(context, baseline, size, expected);
    check_rejected(context, "map20_rejected_before_payload", baseline, size,
                   20, expected);
    check_rejected(context, "truncated_payload_rejected", baseline, size - 1,
                   map_save_current_version(), expected);
    scratch = (unsigned char *)malloc(size);
    for (i = 0; scratch && i < sizeof(cases) / sizeof(cases[0]); i++) {
        int mutated;
        memcpy(scratch, baseline, size);
        mutated = apply_mutation(scratch, cases[i].mutation);
        if (mutated) {
            check_rejected(context, cases[i].name, scratch, size,
                           map_save_current_version(), expected);
        } else {
            war_history_save_probe_check(context, cases[i].name, 0,
                                         "mutation allocation failed");
        }
    }
    if (!scratch) war_history_save_probe_check(context, "mutation_buffer", 0,
                                                "allocation failed");
    free(scratch);
    free(baseline);
    free(expected);
    return context->failures == start_failures;
}
