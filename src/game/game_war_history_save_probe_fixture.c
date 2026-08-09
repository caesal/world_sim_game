#include "game/game_war_history_save_probe_internal.h"

#include "core/game_types.h"
#include "sim/diplomacy.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

void war_history_save_probe_check(WarHistorySaveProbeContext *context,
                                  const char *name, int passed,
                                  const char *detail_format, ...) {
    va_list args;
    if (!context || !context->summary) return;
    context->checks++;
    if (!passed) context->failures++;
    fprintf(context->summary, "%s|%s|", passed ? "PASS" : "FAIL", name);
    va_start(args, detail_format);
    vfprintf(context->summary, detail_format, args);
    va_end(args);
    fputc('\n', context->summary);
}

void war_history_save_probe_setup_civs(void) {
    static const char *names[] = {"Aster", "Beryl", "Cinder", "Dawn"};
    int i;
    memset(civs, 0, sizeof(civs));
    civ_count = 4;
    year = 12;
    month = 6;
    for (i = 0; i < civ_count; i++) {
        civs[i].alive = 1;
        civs[i].uid = 1001 + i;
        civs[i].color = COLOR32_RGB(40 + i * 30, 70 + i * 20, 100 + i * 10);
        snprintf(civs[i].name, sizeof(civs[i].name), "%s", names[i]);
    }
}

static WarHistoryPrincipal principal(int civ_id) {
    WarHistoryPrincipal value;
    memset(&value, 0, sizeof(value));
    value.uid = civs[civ_id].uid;
    value.color = civs[civ_id].color;
    snprintf(value.name_en, sizeof(value.name_en), "%s Realm", civs[civ_id].name);
    snprintf(value.name_zh, sizeof(value.name_zh), "%s ZH", civs[civ_id].name);
    return value;
}

static WarHistoryRecord record_for(int local_civ, int opponent_civ,
                                   uint64_t serial, int result,
                                   int winner_civ, int loser_civ,
                                   int local_losses, int opponent_losses,
                                   int cession, int indemnity,
                                   int beneficiary_civ,
                                   int end_year, int end_month, int duration) {
    WarHistoryRecord record;
    memset(&record, 0, sizeof(record));
    record.war_serial = serial;
    record.local = principal(local_civ);
    record.opponent = principal(opponent_civ);
    record.result = result;
    record.winner_uid = winner_civ >= 0 ? civs[winner_civ].uid : WAR_HISTORY_INVALID_UID;
    record.loser_uid = loser_civ >= 0 ? civs[loser_civ].uid : WAR_HISTORY_INVALID_UID;
    record.local_casualties = local_losses;
    record.opponent_casualties = opponent_losses;
    record.transferred_regions = cession;
    record.indemnity_paid = indemnity;
    record.beneficiary_uid = beneficiary_civ >= 0 ?
                             civs[beneficiary_civ].uid : WAR_HISTORY_INVALID_UID;
    record.end_year = end_year;
    record.end_month = end_month;
    record.duration_months = duration;
    return record;
}

void war_history_save_probe_make_state(WarHistorySaveState *state) {
    if (!state) return;
    war_history_save_probe_setup_civs();
    memset(state, 0, sizeof(*state));
    state->next_serial = 100;
    state->global_revision = 500;
    state->histories[0].owner_uid = civs[0].uid;
    state->histories[0].count = 2;
    state->histories[0].revision = 490;
    state->histories[0].records[0] = record_for(
        0, 1, 90, DIP_LAST_WAR_MILITARY, 0, 1, 111, 222,
        2, 300, 0, 12, 3, 15);
    state->histories[0].records[1] = record_for(
        0, 2, 80, DIP_LAST_WAR_OFFENSIVE_HALTED, 0, 2, 44, 55,
        0, 0, -1, 11, 12, 4);
    state->histories[1].owner_uid = civs[1].uid;
    state->histories[1].count = 1;
    state->histories[1].revision = 491;
    state->histories[1].records[0] = record_for(
        1, 0, 90, DIP_LAST_WAR_MILITARY, 0, 1, 222, 111,
        2, 300, 0, 12, 3, 15);
    state->histories[2].owner_uid = civs[2].uid;
    state->histories[2].count = 1;
    state->histories[2].revision = 492;
    state->histories[2].records[0] = record_for(
        2, 0, 80, DIP_LAST_WAR_OFFENSIVE_HALTED, 0, 2, 55, 44,
        0, 0, -1, 11, 12, 4);
    state->histories[3].owner_uid = civs[3].uid;
    state->histories[3].revision = 493;
}

int war_history_save_probe_state_equal(const WarHistorySaveState *a,
                                       const WarHistorySaveState *b) {
    return a && b && memcmp(a, b, sizeof(*a)) == 0;
}

FILE *war_history_save_probe_file_from_bytes(const unsigned char *bytes,
                                             size_t size) {
    FILE *file = tmpfile();
    if (!file || (size > 0 && fwrite(bytes, 1, size, file) != size)) {
        if (file) fclose(file);
        return NULL;
    }
    rewind(file);
    return file;
}

int war_history_save_probe_read_file(FILE *file, unsigned char **out_bytes,
                                     size_t *out_size) {
    unsigned char *bytes;
    long size;
    if (!file || !out_bytes || !out_size || fseek(file, 0, SEEK_END) != 0) return 0;
    size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) return 0;
    bytes = (unsigned char *)malloc((size_t)size);
    if (!bytes || (size > 0 && fread(bytes, 1, (size_t)size, file) != (size_t)size)) {
        free(bytes);
        return 0;
    }
    *out_bytes = bytes;
    *out_size = (size_t)size;
    return 1;
}

FILE *war_history_save_probe_clone_file(FILE *source) {
    unsigned char *bytes = NULL;
    size_t size = 0;
    FILE *copy = NULL;
    if (war_history_save_probe_read_file(source, &bytes, &size)) {
        copy = war_history_save_probe_file_from_bytes(bytes, size);
    }
    free(bytes);
    return copy;
}
