#include "game/game_war_history_save_probe_internal.h"

#include "core/game_types.h"
#include "io/map_save.h"
#include "io/map_save_state.h"
#include "sim/alliance.h"
#include "sim/diplomacy.h"
#include "sim/plague.h"
#include "sim/war.h"
#include "sim/war_history.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char tag[8];
    int version;
    int item_size;
    int count;
    int aux_a;
    int aux_b;
} ProbeSaveBlockHeader;

typedef enum {
    ACTIVE_BAD_SERIAL,
    ACTIVE_BAD_UID,
    ACTIVE_BAD_START,
    ACTIVE_FUTURE_START,
    ACTIVE_DUPLICATE_SERIAL,
    ACTIVE_COMPLETED_COLLISION,
    ACTIVE_BAD_ITEM_SIZE,
    ACTIVE_BAD_COUNT
} ActiveMutation;

typedef struct {
    const char *name;
    ActiveMutation mutation;
} ActiveMutationCase;

_Static_assert(sizeof(ProbeSaveBlockHeader) == 28,
               "probe dynamic block header mismatch");

static int locate_war_block(FILE *file, long *header_offset,
                            long *payload_offset) {
    int block;
    if (!file || !header_offset || !payload_offset || fseek(file, 0, SEEK_SET) != 0) return 0;
    for (block = 0; block < 8; block++) {
        ProbeSaveBlockHeader header;
        long offset = ftell(file);
        int64_t bytes;
        if (offset < 0 || fread(&header, sizeof(header), 1, file) != 1) return 0;
        if (strncmp(header.tag, "WAR", sizeof(header.tag)) == 0) {
            *header_offset = offset;
            *payload_offset = ftell(file);
            return *payload_offset >= 0;
        }
        if (header.item_size < 0 || header.count < 0) return 0;
        bytes = (int64_t)header.item_size * header.count;
        if (bytes < 0 || bytes > LONG_MAX || fseek(file, (long)bytes, SEEK_CUR) != 0) return 0;
    }
    return 0;
}

static void fill_active_war(ActiveWar *war, int attacker, int defender,
                            uint64_t serial, int start_month) {
    memset(war, 0, sizeof(*war));
    war->active = 1;
    war->attacker = attacker;
    war->defender = defender;
    war->initial_national_a = 500;
    war->initial_national_b = 450;
    war->initial_soldiers_a = 300;
    war->initial_soldiers_b = 275;
    war->soldiers_a = 250;
    war->soldiers_b = 225;
    war->casualties_a = 50;
    war->casualties_b = 50;
    war->war_serial = serial;
    war->attacker_uid = civs[attacker].uid;
    war->defender_uid = civs[defender].uid;
    war->start_absolute_month = start_month;
}

static FILE *write_dynamic_baseline(const WarHistorySaveState *history,
                                    ActiveWar *expected_wars,
                                    int *expected_support) {
    FILE *file;
    diplomacy_reset();
    alliance_reset();
    plague_reset();
    event_log_clear();
    memset(expected_wars, 0, sizeof(*expected_wars) * WAR_SAVE_SLOT_COUNT);
    memset(expected_support, 0, sizeof(*expected_support) * MAX_CIVS);
    fill_active_war(&expected_wars[0], 0, 1, 70, 120);
    expected_support[0] = 17;
    war_restore_save_state(expected_wars, WAR_SAVE_SLOT_COUNT,
                           expected_support, MAX_CIVS, 7);
    if (!war_history_restore_save_state(history)) return NULL;
    file = tmpfile();
    if (!file || !map_save_write_dynamic_state(file)) {
        if (file) fclose(file);
        return NULL;
    }
    return file;
}

static int write_value(FILE *file, long offset, const void *value, size_t size) {
    return file && offset >= 0 && fseek(file, offset, SEEK_SET) == 0 &&
           fwrite(value, size, 1, file) == 1;
}

static int mutate_active_file(FILE *file, long header_offset,
                              long payload_offset, ActiveMutation mutation,
                              const ActiveWar *baseline) {
    ActiveWar duplicate;
    uint64_t serial;
    int value;
    switch (mutation) {
        case ACTIVE_BAD_SERIAL:
            serial = 0;
            return write_value(file, payload_offset + offsetof(ActiveWar, war_serial),
                               &serial, sizeof(serial));
        case ACTIVE_BAD_UID:
            value = baseline->attacker_uid + 100;
            return write_value(file, payload_offset + offsetof(ActiveWar, attacker_uid),
                               &value, sizeof(value));
        case ACTIVE_BAD_START:
            value = -1;
            return write_value(file, payload_offset + offsetof(ActiveWar, start_absolute_month),
                               &value, sizeof(value));
        case ACTIVE_FUTURE_START:
            value = year * 12 + month;
            return write_value(file, payload_offset + offsetof(ActiveWar, start_absolute_month),
                               &value, sizeof(value));
        case ACTIVE_DUPLICATE_SERIAL:
            fill_active_war(&duplicate, 2, 3, baseline->war_serial,
                            baseline->start_absolute_month);
            return write_value(file, payload_offset + (long)sizeof(ActiveWar),
                               &duplicate, sizeof(duplicate));
        case ACTIVE_COMPLETED_COLLISION:
            serial = 90;
            return write_value(file, payload_offset + offsetof(ActiveWar, war_serial),
                               &serial, sizeof(serial));
        case ACTIVE_BAD_ITEM_SIZE:
            value = (int)sizeof(ActiveWar) + 4;
            return write_value(file, header_offset + offsetof(ProbeSaveBlockHeader, item_size),
                               &value, sizeof(value));
        case ACTIVE_BAD_COUNT:
            value = WAR_SAVE_SLOT_COUNT - 1;
            return write_value(file, header_offset + offsetof(ProbeSaveBlockHeader, count),
                               &value, sizeof(value));
    }
    return 0;
}

static void check_active_roundtrip(WarHistorySaveProbeContext *context,
                                   FILE *baseline,
                                   const WarHistorySaveState *history,
                                   const ActiveWar *expected_wars,
                                   const int *expected_support) {
    ActiveWar *actual_wars = (ActiveWar *)calloc(WAR_SAVE_SLOT_COUNT, sizeof(*actual_wars));
    int *actual_support = (int *)calloc(MAX_CIVS, sizeof(*actual_support));
    int total_started = 0;
    int read_result;
    war_reset();
    war_history_restore_save_state(history);
    rewind(baseline);
    read_result = map_save_read_dynamic_state_with_history(
        baseline, map_save_current_version(), history);
    if (actual_wars && actual_support) {
        war_copy_save_state(actual_wars, WAR_SAVE_SLOT_COUNT,
                            actual_support, MAX_CIVS, &total_started);
    }
    war_history_save_probe_check(context, "active_serial_start_exact_roundtrip",
        read_result == 1 && actual_wars && actual_support &&
        memcmp(actual_wars, expected_wars,
               sizeof(*actual_wars) * WAR_SAVE_SLOT_COUNT) == 0 &&
        memcmp(actual_support, expected_support,
               sizeof(*actual_support) * MAX_CIVS) == 0 && total_started == 7,
        "read=%d serial=%llu start=%d total_started=%d",
        read_result,
        (unsigned long long)(actual_wars ? actual_wars[0].war_serial : 0),
        actual_wars ? actual_wars[0].start_absolute_month : -1,
        total_started);
    free(actual_wars);
    free(actual_support);
}

int game_war_history_save_probe_dynamic(WarHistorySaveProbeContext *context) {
    static const ActiveMutationCase cases[] = {
        {"active_zero_serial_rejected", ACTIVE_BAD_SERIAL},
        {"active_uid_mismatch_rejected", ACTIVE_BAD_UID},
        {"active_negative_start_rejected", ACTIVE_BAD_START},
        {"active_future_start_rejected", ACTIVE_FUTURE_START},
        {"active_duplicate_serial_rejected", ACTIVE_DUPLICATE_SERIAL},
        {"active_completed_serial_collision_rejected", ACTIVE_COMPLETED_COLLISION},
        {"active_item_size_mismatch_rejected", ACTIVE_BAD_ITEM_SIZE},
        {"active_slot_count_mismatch_rejected", ACTIVE_BAD_COUNT}
    };
    WarHistorySaveState *history = (WarHistorySaveState *)calloc(1, sizeof(*history));
    ActiveWar *expected_wars = (ActiveWar *)calloc(WAR_SAVE_SLOT_COUNT, sizeof(*expected_wars));
    int *expected_support = (int *)calloc(MAX_CIVS, sizeof(*expected_support));
    FILE *baseline = NULL;
    long header_offset = -1;
    long payload_offset = -1;
    size_t i;
    int start_failures = context->failures;
    if (!history || !expected_wars || !expected_support) goto allocation_failed;
    war_history_save_probe_make_state(history);
    baseline = write_dynamic_baseline(history, expected_wars, expected_support);
    if (!baseline || !locate_war_block(baseline, &header_offset, &payload_offset)) {
        war_history_save_probe_check(context, "write_dynamic_map21_baseline", 0,
                                     "write or WAR block lookup failed");
        goto done;
    }
    war_history_save_probe_check(context, "write_dynamic_map21_baseline", 1,
                                 "header_offset=%ld payload_offset=%ld active_size=%u",
                                 header_offset, payload_offset, (unsigned)sizeof(ActiveWar));
    check_active_roundtrip(context, baseline, history, expected_wars, expected_support);
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        FILE *variant = war_history_save_probe_clone_file(baseline);
        int mutated = variant && mutate_active_file(
            variant, header_offset, payload_offset, cases[i].mutation, &expected_wars[0]);
        int read_result = -2;
        if (mutated) {
            rewind(variant);
            read_result = map_save_read_dynamic_state_with_history(
                variant, map_save_current_version(), history);
        }
        war_history_save_probe_check(context, cases[i].name,
            mutated && read_result == -1,
            "mutated=%d read=%d", mutated, read_result);
        if (variant) fclose(variant);
    }
    goto done;
allocation_failed:
    war_history_save_probe_check(context, "dynamic_probe_allocation", 0,
                                 "allocation failed");
done:
    if (baseline) fclose(baseline);
    free(expected_support);
    free(expected_wars);
    free(history);
    return context->failures == start_failures;
}
