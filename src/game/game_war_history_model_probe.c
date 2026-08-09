#include "game/game_war_history_model_probe.h"

#include "core/game_state.h"
#include "game/game_war_history_probe_fixture.h"
#include "sim/civilization_slots.h"
#include "sim/diplomacy.h"
#include "sim/simulation.h"
#include "sim/war.h"
#include "sim/war_history.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static int copy_history(int civ_id, WarHistory *history) {
    return war_history_copy_for_civ(civ_id, civs[civ_id].uid, history);
}

static int case_capacity_order_idempotence(FILE *out) {
    uint64_t serials[4];
    uint64_t duplicate_revision;
    WarHistory history = {0};
    WarHistoryRecord record;
    int ok = 1;
    int i;
    war_history_probe_fixture_reset(2);
    ok &= copy_history(0, &history) && history.count == 0 &&
          history.owner_uid == civs[0].uid;
    for (i = 0; i < 4; i++) {
        serials[i] = war_history_allocate_serial();
        record = war_history_probe_fixture_record(
            0, 1, serials[i], DIP_LAST_WAR_MILITARY,
            civs[0].uid, civs[1].uid);
        ok &= war_history_append(0, &record);
        ok &= copy_history(0, &history);
        ok &= history.count == min(i + 1, WAR_HISTORY_CAPACITY);
        ok &= history.records[0].war_serial == serials[i];
    }
    ok &= history.count == 3 && history.records[0].war_serial == serials[3] &&
          history.records[1].war_serial == serials[2] &&
          history.records[2].war_serial == serials[1];
    duplicate_revision = history.revision;
    ok &= !war_history_append(0, &record);
    ok &= copy_history(0, &history) && history.revision == duplicate_revision &&
          history.count == 3;
    fprintf(out,
            "case=capacity_order_idempotence ok=%d counts=0/1/2/3/3 newest=%llu/%llu/%llu evicted=%llu duplicate_revision=%llu\n",
            ok, (unsigned long long)history.records[0].war_serial,
            (unsigned long long)history.records[1].war_serial,
            (unsigned long long)history.records[2].war_serial,
            (unsigned long long)serials[0],
            (unsigned long long)duplicate_revision);
    return ok;
}

static int case_principal_perspectives(FILE *out) {
    WarHistoryRecord local_a;
    WarHistoryRecord local_b;
    WarHistory a = {0};
    WarHistory b = {0};
    uint64_t serial;
    int ok;
    war_history_probe_fixture_reset(3);
    serial = war_history_allocate_serial();
    local_a = war_history_probe_fixture_record(0, 1, serial, DIP_LAST_WAR_SURRENDER,
                                               civs[0].uid, civs[1].uid);
    local_b = war_history_probe_fixture_record(1, 0, serial, DIP_LAST_WAR_SURRENDER,
                                               civs[0].uid, civs[1].uid);
    ok = war_history_append(0, &local_a) && war_history_append(1, &local_b) &&
         copy_history(0, &a) && copy_history(1, &b);
    ok &= a.count == 1 && b.count == 1 &&
          a.records[0].war_serial == b.records[0].war_serial &&
          a.records[0].local.uid == civs[0].uid &&
          a.records[0].opponent.uid == civs[1].uid &&
          b.records[0].local.uid == civs[1].uid &&
          b.records[0].opponent.uid == civs[0].uid &&
          a.records[0].winner_uid == b.records[0].winner_uid &&
          a.records[0].loser_uid == b.records[0].loser_uid;
    ok &= copy_history(2, &a) && a.count == 0;
    fprintf(out,
            "case=principal_perspectives ok=%d serial=%llu left_uids=%d/%d supporter_count=%d\n",
            ok, (unsigned long long)serial, local_a.local.uid, local_b.local.uid,
            a.count);
    return ok;
}

static int case_frozen_identity_and_reuse(FILE *out) {
    WarHistoryRecord record;
    WarHistory history = {0};
    char frozen_name[NAME_LEN];
    Color32 frozen_color;
    uint64_t serial;
    int old_uid;
    int new_uid;
    int frozen_ok;
    int reuse_ok;
    war_history_probe_fixture_reset(2);
    old_uid = civs[0].uid;
    serial = war_history_allocate_serial();
    record = war_history_probe_fixture_record(0, 1, serial, DIP_LAST_WAR_MILITARY,
                                              civs[0].uid, civs[1].uid);
    snprintf(frozen_name, sizeof(frozen_name), "%s", record.local.name_en);
    frozen_color = record.local.color;
    war_history_append(0, &record);
    snprintf(civs[0].name, sizeof(civs[0].name), "Renamed after war");
    civs[0].color = COLOR32_RGB(2, 3, 4);
    civs[0].alive = 0;
    frozen_ok = war_history_copy_for_civ(0, old_uid, &history) && history.count == 1 &&
        strcmp(history.records[0].local.name_en, frozen_name) == 0 &&
        history.records[0].local.color == frozen_color;
    civilization_reset_slot_state(0);
    new_uid = civs[0].uid;
    civs[0].alive = 1;
    reuse_ok = new_uid != old_uid &&
        !war_history_copy_for_civ(0, old_uid, &history) &&
        war_history_copy_for_civ(0, new_uid, &history) && history.count == 0 &&
        history.owner_uid == new_uid;
    fprintf(out,
            "case=frozen_identity_death_slot_reuse ok=%d frozen=%d reuse=%d uid=%d/%d name=%s\n",
            frozen_ok && reuse_ok, frozen_ok, reuse_ok, old_uid, new_uid, frozen_name);
    return frozen_ok && reuse_ok;
}

static int all_current_slots_empty_and_bound(void) {
    WarHistory history = {0};
    int civ_id;
    for (civ_id = 0; civ_id < civ_count; civ_id++) {
        if (!war_history_copy_for_civ(civ_id, civs[civ_id].uid, &history) ||
            history.owner_uid != civs[civ_id].uid || history.count != 0) return 0;
    }
    return 1;
}

static int all_storage_empty(void) {
    WarHistorySaveState state;
    int civ_id;
    war_history_copy_save_state(&state);
    for (civ_id = 0; civ_id < MAX_CIVS; civ_id++) {
        if (state.histories[civ_id].owner_uid != WAR_HISTORY_INVALID_UID ||
            state.histories[civ_id].count != 0) return 0;
    }
    return 1;
}

static int case_reset_worldgen_first_binding(FILE *out) {
    WarHistoryRecord record;
    int trailing_reset_ok;
    int storage_reset_ok;
    int first_binding_ok;
    war_history_probe_fixture_reset(3);
    record = war_history_probe_fixture_record(
        0, 1, war_history_allocate_serial(), DIP_LAST_WAR_MILITARY,
        civs[0].uid, civs[1].uid);
    war_history_append(0, &record);
    war_reset();
    trailing_reset_ok = all_current_slots_empty_and_bound();
    simulation_reset_state();
    storage_reset_ok = civ_count == 0 && all_storage_empty();
    war_history_probe_fixture_reset(3);
    first_binding_ok = all_current_slots_empty_and_bound() &&
                       war_history_next_serial() == 1;
    fprintf(out,
            "case=reset_worldgen_first_binding ok=%d trailing_war_reset=%d storage_reset=%d first_binding=%d civs=%d\n",
            trailing_reset_ok && storage_reset_ok && first_binding_ok,
            trailing_reset_ok, storage_reset_ok, first_binding_ok, civ_count);
    return trailing_reset_ok && storage_reset_ok && first_binding_ok;
}

static int case_fixed_work_and_rng(FILE *out) {
    const int copy_iterations = 100000;
    const int append_iterations = 20000;
    LARGE_INTEGER frequency;
    LARGE_INTEGER start;
    LARGE_INTEGER after_copy;
    LARGE_INTEGER after_append;
    WarHistory history;
    WarHistoryRecord record;
    uint64_t revision_before;
    unsigned int checksum = 0;
    double copy_us;
    double append_us;
    int expected_rng;
    int actual_rng;
    int ok = 1;
    int i;
    war_history_probe_fixture_reset(2);
    record = war_history_probe_fixture_record(
        0, 1, war_history_allocate_serial(), DIP_LAST_WAR_MILITARY,
        civs[0].uid, civs[1].uid);
    ok &= war_history_append(0, &record);
    srand(712367u);
    expected_rng = rand();
    srand(712367u);
    ok &= copy_history(0, &history);
    actual_rng = rand();
    ok &= actual_rng == expected_rng;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    for (i = 0; i < copy_iterations; i++) {
        ok &= copy_history(0, &history);
        checksum += (unsigned int)history.count;
    }
    QueryPerformanceCounter(&after_copy);
    revision_before = war_history_revision();
    for (i = 0; i < append_iterations; i++) {
        record.war_serial = war_history_allocate_serial();
        ok &= war_history_append(0, &record);
    }
    QueryPerformanceCounter(&after_append);
    ok &= copy_history(0, &history) && history.count == WAR_HISTORY_CAPACITY &&
          war_history_revision() == revision_before + (uint64_t)append_iterations;
    copy_us = (double)(after_copy.QuadPart - start.QuadPart) * 1000000.0 /
              (double)frequency.QuadPart / copy_iterations;
    append_us = (double)(after_append.QuadPart - after_copy.QuadPart) * 1000000.0 /
                (double)frequency.QuadPart / append_iterations;
    fprintf(out,
            "case=fixed_work_rng ok=%d capacity=%d copy_iterations=%d copy_mean_us=%.6f append_iterations=%d append_mean_us=%.6f checksum=%u rng=%d/%d\n",
            ok, history.count, copy_iterations, copy_us, append_iterations,
            append_us, checksum, expected_rng, actual_rng);
    return ok;
}

int run_war_history_model_probe_cases(FILE *out) {
    int ok = 1;
    ok &= case_capacity_order_idempotence(out);
    ok &= case_principal_perspectives(out);
    ok &= case_frozen_identity_and_reuse(out);
    ok &= case_reset_worldgen_first_binding(out);
    ok &= case_fixed_work_and_rng(out);
    return ok;
}
