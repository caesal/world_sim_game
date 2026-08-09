#include "game/game_presentation_war_history_fixture.h"

#include "sim/diplomacy.h"

#include <stdio.h>
#include <string.h>

static const WarHistoryPresentationCase presentation_cases[] = {
    {"c0_active", 0, 0, 1, DIP_LAST_WAR_NONE, 0,
     WAR_HISTORY_SETTLEMENT_NONE, WAR_HISTORY_BENEFICIARY_NONE, 0},
    {"c1_win_cession_left", 199, 1, 0, DIP_LAST_WAR_DECISIVE, 1,
     WAR_HISTORY_SETTLEMENT_CESSION, WAR_HISTORY_BENEFICIARY_LOCAL, 0},
    {"c2_defeat_cession_right", 0, 2, 0, DIP_LAST_WAR_MILITARY, -1,
     WAR_HISTORY_SETTLEMENT_CESSION, WAR_HISTORY_BENEFICIARY_OPPONENT, 0},
    {"c3_surrender_indemnity_left", 199, 3, 0, DIP_LAST_WAR_SURRENDER, 1,
     WAR_HISTORY_SETTLEMENT_INDEMNITY, WAR_HISTORY_BENEFICIARY_LOCAL, 0},
    {"c3_negotiated_active", 0, 3, 1, DIP_LAST_WAR_NEGOTIATED_TRUCE, 0,
     WAR_HISTORY_SETTLEMENT_NONE, WAR_HISTORY_BENEFICIARY_NONE, 0},
    {"c3_enemy_halted", 199, 3, 0,
     DIP_LAST_WAR_OFFENSIVE_HALTED, -1,
     WAR_HISTORY_SETTLEMENT_NONE, WAR_HISTORY_BENEFICIARY_NONE, 0},
    {"c3_front_severed_long", 0, 3, 0, DIP_LAST_WAR_FRONT_SEVERED, 0,
     WAR_HISTORY_SETTLEMENT_NONE, WAR_HISTORY_BENEFICIARY_NONE, 1},
    {"c3_surrender_defeat_active", 199, 3, 1, DIP_LAST_WAR_SURRENDER, -1,
     WAR_HISTORY_SETTLEMENT_CESSION | WAR_HISTORY_SETTLEMENT_INDEMNITY,
     WAR_HISTORY_BENEFICIARY_OPPONENT, 1},
    {"c3_offensive_halted_win", 0, 3, 0,
     DIP_LAST_WAR_OFFENSIVE_HALTED, 1,
     WAR_HISTORY_SETTLEMENT_NONE, WAR_HISTORY_BENEFICIARY_NONE, 0},
    {"c3_interrupted", 199, 3, 0, DIP_LAST_WAR_INTERRUPTED, 0,
     WAR_HISTORY_SETTLEMENT_NONE, WAR_HISTORY_BENEFICIARY_NONE, 0}
};

static int local_uid_for(int selected_civ) {
    return 51000 + selected_civ;
}

static int opponent_uid_for(int selected_civ, int record_index) {
    return 62000 + selected_civ * 4 + record_index;
}

static void fill_civ(SnapshotCiv *civ, int id, int uid,
                     const char *name_en, const char *name_zh,
                     Color32 color) {
    memset(civ, 0, sizeof(*civ));
    civ->alive = 1;
    civ->id = id;
    civ->uid = uid;
    civ->overlord = -1;
    civ->color = color;
    civ->symbol = (char)('A' + id % 26);
    civ->current_soldiers = 987654;
    civ->war_deployed_soldiers = 456789;
    civ->war_available_reserve = 530865;
    civ->summary.population = 87654321;
    snprintf(civ->name_en, sizeof(civ->name_en), "%s", name_en);
    snprintf(civ->name_zh, sizeof(civ->name_zh), "%s", name_zh);
}

static void fill_principal(WarHistoryPrincipal *principal, int uid,
                           const char *name_en, const char *name_zh,
                           Color32 color) {
    memset(principal, 0, sizeof(*principal));
    principal->uid = uid;
    principal->color = color;
    snprintf(principal->name_en, sizeof(principal->name_en), "%s", name_en);
    snprintf(principal->name_zh, sizeof(principal->name_zh), "%s", name_zh);
}

static void set_perspective(WarHistoryRecord *record, int local_wins) {
    if (local_wins > 0) {
        record->winner_uid = record->local.uid;
        record->loser_uid = record->opponent.uid;
    } else if (local_wins < 0) {
        record->winner_uid = record->opponent.uid;
        record->loser_uid = record->local.uid;
    } else {
        record->winner_uid = WAR_HISTORY_INVALID_UID;
        record->loser_uid = WAR_HISTORY_INVALID_UID;
    }
}

static void set_settlement(WarHistoryRecord *record, int settlement,
                           int beneficiary, int long_values) {
    record->transferred_regions =
        settlement & WAR_HISTORY_SETTLEMENT_CESSION ?
        (long_values ? 1536 : 7) : 0;
    record->indemnity_paid =
        settlement & WAR_HISTORY_SETTLEMENT_INDEMNITY ?
        (long_values ? 987654321 : 42000) : 0;
    if (beneficiary == WAR_HISTORY_BENEFICIARY_LOCAL) {
        record->beneficiary_uid = record->local.uid;
    } else if (beneficiary == WAR_HISTORY_BENEFICIARY_OPPONENT) {
        record->beneficiary_uid = record->opponent.uid;
    } else {
        record->beneficiary_uid = WAR_HISTORY_INVALID_UID;
    }
}

static void fill_record(WarHistoryRecord *record, int selected_civ,
                        int case_index, int record_index, int result,
                        int local_wins, int settlement, int beneficiary,
                        int long_values) {
    const char *local_en = long_values ?
        "Commonwealth of the Far Western Meridian" :
        (selected_civ == 0 ? "Early Meridian" : "Late Meridian");
    const char *local_zh = long_values ?
        "遥远西部经线共同体" : (selected_civ == 0 ? "前期经线国" : "后期经线国");
    const char *opponent_en = long_values ?
        "Federation of the Northern Sapphire Archipelago" :
        (record_index == 0 ? "Azure Reach" :
         record_index == 1 ? "Copper March" : "Ivory Coastland");
    const char *opponent_zh = long_values ?
        "北方蓝宝石群岛联邦" :
        (record_index == 0 ? "蔚蓝疆域" :
         record_index == 1 ? "赤铜边疆" : "象牙海岸国");
    memset(record, 0, sizeof(*record));
    record->war_serial = UINT64_C(900000) + case_index * 10 +
                         (WAR_HISTORY_CAPACITY - record_index);
    fill_principal(&record->local, local_uid_for(selected_civ),
                   local_en, local_zh, COLOR32_RGB(74, 151, 186));
    fill_principal(&record->opponent,
                   opponent_uid_for(selected_civ, record_index),
                   opponent_en, opponent_zh,
                   COLOR32_RGB(181 - record_index * 24,
                               93 + record_index * 21,
                               87 + record_index * 31));
    record->result = result;
    set_perspective(record, local_wins);
    record->local_casualties = long_values ? 987654321 :
                                 14000 + record_index * 3111;
    record->opponent_casualties = long_values ? 876543210 :
                                    21000 + record_index * 4222;
    set_settlement(record, settlement, beneficiary, long_values);
    record->end_year = 42 - (record_index == 2);
    record->end_month = record_index == 0 ? 7 :
                        record_index == 1 ? 3 : 12;
    record->duration_months = long_values ? 9999 : 5 + record_index * 13;
}

static void fill_history(RenderSnapshot *snapshot,
                         const WarHistoryPresentationCase *spec,
                         int case_index) {
    SnapshotCiv *civ = &snapshot->civs[spec->selected_civ];
    WarHistory *history = &civ->war_history;
    history->owner_uid = civ->uid;
    history->count = spec->history_count;
    history->revision = UINT64_C(8000000000) + case_index;
    if (spec->history_count > 0) {
        fill_record(&history->records[0], spec->selected_civ, case_index, 0,
                    spec->lead_result, spec->lead_perspective,
                    spec->lead_settlement,
                    spec->lead_beneficiary, spec->long_values);
    }
    if (spec->history_count > 1) {
        fill_record(&history->records[1], spec->selected_civ, case_index, 1,
                    DIP_LAST_WAR_NEGOTIATED_TRUCE, 0,
                    WAR_HISTORY_SETTLEMENT_NONE,
                    WAR_HISTORY_BENEFICIARY_NONE, 0);
    }
    if (spec->history_count > 2) {
        int result = case_index == 3 ? DIP_LAST_WAR_OFFENSIVE_HALTED :
                     case_index == 4 ? DIP_LAST_WAR_DECISIVE :
                     case_index == 5 ? DIP_LAST_WAR_SURRENDER :
                     case_index == 6 ? DIP_LAST_WAR_MILITARY :
                                       DIP_LAST_WAR_FRONT_SEVERED;
        int wins = case_index == 4 || case_index == 6 ? 1 :
                   case_index == 5 ? -1 : 0;
        fill_record(&history->records[2], spec->selected_civ, case_index, 2,
                    result, wins, WAR_HISTORY_SETTLEMENT_NONE,
                    WAR_HISTORY_BENEFICIARY_NONE, spec->long_values);
    }
}

static void fill_active_war(RenderSnapshot *snapshot, int selected_civ) {
    int other_id = selected_civ == 0 ? 1 : 198;
    SnapshotWar war = {0};
    fill_civ(&snapshot->civs[other_id], other_id, 73000 + other_id,
             "Current Frontier Coalition", "当前边疆联盟",
             COLOR32_RGB(183, 106, 92));
    snapshot->relations[selected_civ][other_id].state = DIPLOMACY_WAR;
    snapshot->relations[other_id][selected_civ].state = DIPLOMACY_WAR;
    snapshot->relations[selected_civ][other_id].relation_score = -91;
    snapshot->relations[other_id][selected_civ].relation_score = -88;
    war.active = 1;
    war.attacker = selected_civ;
    war.defender = other_id;
    war.soldiers_a = 180000;
    war.soldiers_b = 165000;
    war.casualties_a = 22000;
    war.casualties_b = 31000;
    war.temporary_soldiers_a = 18000;
    war.temporary_soldiers_b = 11000;
    war.alliance_reinforcements_a = 9000;
    war.alliance_reinforcements_b = 7000;
    war.wins_a = 3;
    war.wins_b = 2;
    war.years = 2;
    snapshot->wars[selected_civ][other_id] = war;
    snapshot->wars[other_id][selected_civ] = war;
    snapshot->war_peace_pressure[selected_civ][other_id] = 46;
    snapshot->war_peace_pressure[other_id][selected_civ] = 61;
    snapshot->civs[selected_civ].war_active = 1;
    snapshot->civs[selected_civ].war_front_count = 1;
}

const WarHistoryPresentationCase *game_presentation_war_history_case(
    int case_index) {
    if (case_index < 0 || case_index >= WAR_HISTORY_PRESENTATION_CASE_COUNT) {
        return NULL;
    }
    return &presentation_cases[case_index];
}

int game_presentation_war_history_matrix_contract(void) {
    int counts = 0;
    int results = 0;
    int settlements = 0;
    int beneficiaries = 0;
    int activity = 0;
    int identities = 0;
    int long_values = 0;
    int i;
    for (i = 0; i < WAR_HISTORY_PRESENTATION_CASE_COUNT; i++) {
        const WarHistoryPresentationCase *spec = &presentation_cases[i];
        counts |= 1 << spec->history_count;
        settlements |= 1 << spec->lead_settlement;
        beneficiaries |= 1 << spec->lead_beneficiary;
        activity |= 1 << spec->active_war;
        identities |= spec->selected_civ == 0 ? 1 : 2;
        long_values |= spec->long_values;
        if ((spec->lead_result == DIP_LAST_WAR_DECISIVE ||
             spec->lead_result == DIP_LAST_WAR_MILITARY) &&
            spec->lead_perspective > 0) results |= 1;
        if ((spec->lead_result == DIP_LAST_WAR_DECISIVE ||
             spec->lead_result == DIP_LAST_WAR_MILITARY) &&
            spec->lead_perspective < 0) results |= 2;
        if (spec->lead_result == DIP_LAST_WAR_SURRENDER &&
            spec->lead_perspective > 0) results |= 4;
        if (spec->lead_result == DIP_LAST_WAR_SURRENDER &&
            spec->lead_perspective < 0) results |= 8;
        if (spec->lead_result == DIP_LAST_WAR_NEGOTIATED_TRUCE) results |= 16;
        if (spec->lead_result == DIP_LAST_WAR_OFFENSIVE_HALTED) results |= 32;
        if (spec->lead_result == DIP_LAST_WAR_FRONT_SEVERED) results |= 64;
        if (spec->lead_result == DIP_LAST_WAR_INTERRUPTED) results |= 128;
    }
    return counts == 15 && results == 255 && settlements == 15 &&
           beneficiaries == 7 && activity == 3 && identities == 3 &&
           long_values;
}

void game_presentation_war_history_fill_fixture(
    RenderSnapshot *snapshot, int case_index) {
    const WarHistoryPresentationCase *spec =
        game_presentation_war_history_case(case_index);
    int i;
    if (!snapshot || !spec) return;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->world_generated = 1;
    snapshot->map_w = 96;
    snapshot->map_h = 64;
    snapshot->year = 42;
    snapshot->month = 7;
    snapshot->civ_count = MAX_CIVS;
    snapshot->civ_alive_count = 1 + spec->active_war;
    snapshot->civs_revision = 4100 + case_index;
    snapshot->diplomacy_revision = 5100 + case_index;
    snapshot->revision = 6100 + case_index;
    for (i = 0; i < MAX_CIVS; i++) snapshot->civs[i].overlord = -1;
    fill_civ(&snapshot->civs[spec->selected_civ], spec->selected_civ,
             local_uid_for(spec->selected_civ),
             spec->selected_civ == 0 ? "Early Meridian" : "Late Meridian",
             spec->selected_civ == 0 ? "前期经线国" : "后期经线国",
             COLOR32_RGB(74, 151, 186));
    fill_history(snapshot, spec, case_index);
    if (spec->active_war) fill_active_war(snapshot, spec->selected_civ);
}

int game_presentation_war_history_fixture_contract(
    const RenderSnapshot *snapshot, int case_index) {
    const WarHistoryPresentationCase *spec =
        game_presentation_war_history_case(case_index);
    const SnapshotCiv *civ;
    const SnapshotWarHistory *history;
    int i;
    if (!snapshot || !spec) return 0;
    civ = &snapshot->civs[spec->selected_civ];
    history = &civ->war_history;
    if (!civ->alive || history->owner_uid != civ->uid ||
        history->count != spec->history_count || history->count < 0 ||
        history->count > WAR_HISTORY_CAPACITY) return 0;
    if (history->count > 0) {
        const SnapshotWarHistoryRecord *lead = &history->records[0];
        int settlement =
            (lead->transferred_regions > 0 ? WAR_HISTORY_SETTLEMENT_CESSION : 0) |
            (lead->indemnity_paid > 0 ? WAR_HISTORY_SETTLEMENT_INDEMNITY : 0);
        int beneficiary = lead->beneficiary_uid == lead->local.uid ?
                          WAR_HISTORY_BENEFICIARY_LOCAL :
                          lead->beneficiary_uid == lead->opponent.uid ?
                          WAR_HISTORY_BENEFICIARY_OPPONENT :
                          WAR_HISTORY_BENEFICIARY_NONE;
        int perspective = lead->winner_uid == lead->local.uid ? 1 :
                          lead->winner_uid == lead->opponent.uid ? -1 : 0;
        if (lead->result != spec->lead_result ||
            perspective != spec->lead_perspective ||
            settlement != spec->lead_settlement ||
            beneficiary != spec->lead_beneficiary) return 0;
    }
    for (i = 0; i < history->count; i++) {
        const SnapshotWarHistoryRecord *record = &history->records[i];
        int has_settlement = record->transferred_regions > 0 ||
                             record->indemnity_paid > 0;
        if (record->local.uid != civ->uid ||
            record->opponent.uid == WAR_HISTORY_INVALID_UID ||
            record->opponent.uid == record->local.uid ||
            record->local_casualties < 0 || record->opponent_casualties < 0 ||
            record->duration_months < 1 || record->end_month < 1 ||
            record->end_month > 12) return 0;
        if (i > 0 && history->records[i - 1].war_serial <=
                     record->war_serial) return 0;
        if (!has_settlement && record->beneficiary_uid !=
                               WAR_HISTORY_INVALID_UID) return 0;
        if (has_settlement && record->beneficiary_uid != record->local.uid &&
            record->beneficiary_uid != record->opponent.uid) return 0;
    }
    return spec->active_war ? civ->war_front_count == 1 :
                              civ->war_front_count == 0;
}
