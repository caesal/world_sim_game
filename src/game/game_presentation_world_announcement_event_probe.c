#include "game/game_presentation_world_announcement_probe.h"

#include "core/event_log_store.h"
#include "core/game_notifications.h"
#include "core/game_state.h"
#include "core/world_announcement_store.h"
#include "sim/alliance.h"
#include "sim/diplomacy.h"
#include "sim/plague.h"
#include "sim/simulation.h"
#include "sim/technology.h"
#include "sim/war.h"
#include "sim/world_announcement.h"
#include "ui/ui_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIXTURE_CIVS 16

typedef struct {
    int civ_count, city_count, saved_year, saved_month;
    Civilization civs[FIXTURE_CIVS];
    City city0;
    AllianceSaveState *alliance;
    PlagueState *plagues;
    int *route_exposure;
    int plague_last_city;
} AnnouncementFixtureBackup;

static int latest(WorldAnnouncementEvent *event) {
    return world_announcement_store_copy_newest(event, 1) == 1;
}

static void set_alliance_record(AllianceSaveState *state, int id, int founder,
                                const int *members, int count, int military) {
    int i;
    AllianceRecord *record = &state->records[id];
    record->active = 1;
    record->id = id;
    record->founder_civ_id = founder;
    record->member_count = count;
    snprintf(record->name_en, sizeof(record->name_en), "%s", id ? "Northern Accord" : "Aegean Compact");
    snprintf(record->name_zh, sizeof(record->name_zh), "%s", id ? "北方协约" : "爱琴海公约");
    for (i = 0; i < count; i++) {
        record->members[i] = members[i];
        state->civ_alliance[members[i]] = id;
    }
    state->alliance_type[id] = military ? ALLIANCE_TYPE_MILITARY : ALLIANCE_TYPE_DEFENSIVE;
}

static void install_alliance_fixture(AllianceSaveState *state) {
    int a[] = {0, 2};
    int b[] = {1};
    int i;
    memset(state, 0, sizeof(*state));
    for (i = 0; i < MAX_CIVS; i++) state->civ_alliance[i] = -1;
    state->next_id = 2;
    set_alliance_record(state, 0, 0, a, 2, 1);
    set_alliance_record(state, 1, 1, b, 1, 1);
    alliance_restore_save_state(state);
}

static int fixture_begin(AnnouncementFixtureBackup *backup, AllianceSaveState *work) {
    int i;
    memset(backup, 0, sizeof(*backup));
    backup->alliance = malloc(sizeof(*backup->alliance));
    backup->plagues = malloc(sizeof(*backup->plagues) * MAX_CITIES);
    backup->route_exposure = malloc(sizeof(*backup->route_exposure) * MAX_MARITIME_ROUTES);
    if (!backup->alliance || !backup->plagues || !backup->route_exposure) {
        free(backup->route_exposure);
        free(backup->plagues);
        free(backup->alliance);
        return 0;
    }
    backup->civ_count = civ_count;
    backup->city_count = city_count;
    backup->saved_year = year;
    backup->saved_month = month;
    memcpy(backup->civs, civs, sizeof(backup->civs));
    backup->city0 = cities[0];
    alliance_copy_save_state(backup->alliance);
    plague_copy_save_state(backup->plagues, MAX_CITIES, backup->route_exposure,
                           MAX_MARITIME_ROUTES, &backup->plague_last_city);
    civ_count = FIXTURE_CIVS;
    city_count = max(city_count, 1);
    year = 88;
    month = 7;
    for (i = 0; i < FIXTURE_CIVS; i++) {
        memset(&civs[i], 0, sizeof(civs[i]));
        civs[i].alive = 1;
        civs[i].uid = 4100 + i;
        civs[i].symbol = (char)('A' + i);
        civs[i].color = COLOR32_RGB(38 + i * 13 % 180, 54 + i * 31 % 170, 70 + i * 47 % 160);
        civs[i].capital_city = -1;
        civilization_assign_generated_name_for_heritage(&civs[i], i % CIV_HERITAGE_COUNT, i);
    }
    memset(&cities[0], 0, sizeof(cities[0]));
    cities[0].alive = 1;
    cities[0].owner = 0;
    cities[0].x = 12;
    cities[0].y = 8;
    snprintf(cities[0].name, sizeof(cities[0].name), "Meridian Harbor");
    civs[0].capital_city = 0;
    install_alliance_fixture(work);
    plague_reset();
    event_log_clear();
    world_announcement_state_reset();
    return 1;
}

static void fixture_end(AnnouncementFixtureBackup *backup) {
    plague_restore_save_state(backup->plagues, MAX_CITIES, backup->route_exposure,
                              MAX_MARITIME_ROUTES, backup->plague_last_city);
    alliance_restore_save_state(backup->alliance);
    memcpy(civs, backup->civs, sizeof(backup->civs));
    cities[0] = backup->city0;
    civ_count = backup->civ_count;
    city_count = backup->city_count;
    year = backup->saved_year;
    month = backup->saved_month;
    free(backup->route_exposure);
    free(backup->plagues);
    free(backup->alliance);
}

static int case_age(FILE *summary, WorldAnnouncementProbeBundle *bundle) {
    int i, duplicate_count, load_replay, named = 1;
    event_log_clear();
    world_announcement_state_reset();
    world_announcement_emit_age_first(0, 0);
    for (i = 1; i <= 10; i++) {
        world_announcement_emit_age_first(0, i);
        if (i == 5) latest(&bundle->age);
        named &= technology_stage_name(i, UI_LANG_EN)[0] && technology_stage_name(i, UI_LANG_ZH)[0] &&
                 strstr(technology_stage_name(i, UI_LANG_EN), "Stage") == NULL;
        world_announcement_emit_age_first(1, i);
    }
    duplicate_count = world_announcement_store_count() - 10;
    event_log_clear();
    world_announcement_state_reset();
    civs[0].tech_stage = 10;
    world_announcement_state_baseline_from_world();
    for (i = 1; i <= 10; i++) world_announcement_emit_age_first(1, i);
    load_replay = world_announcement_store_count();
    fprintf(summary,
        "case=world_announcement_age_first ok=%d stages=%d stage0_suppressed=%d named_ages=%d numeric_suffix=%d duplicate=%d load_replay=%d\n",
        duplicate_count == 0 && load_replay == 0 && named && bundle->age.technology_stage == 5,
        10, duplicate_count == 0, named, bundle->age.technology_stage == 5,
        duplicate_count, load_replay);
    civs[0].tech_stage = 0;
    return duplicate_count == 0 && load_replay == 0 && named && bundle->age.event_id > 0;
}

static int case_collapse_union(WorldAnnouncementProbeBundle *bundle, AllianceSaveState *work) {
    int successors[] = {1, 2, 3, 4, 5};
    int members_four[] = {0, 1, 2, 3, 4};
    int members_long[13];
    int i, stable, notifications = game_notifications_count();
    Color32 captured;
    event_log_clear();
    world_announcement_emit_collapse(0, -1, -1, successors, 5, 5);
    latest(&bundle->collapse);
    captured = bundle->collapse.related[0].color;
    civs[1].color = COLOR32_RGB(1, 2, 3);
    stable = bundle->collapse.related[0].color == captured;
    civs[1].color = captured;
    bundle->collapse_stable = stable;
    event_log_clear();
    install_alliance_fixture(work);
    world_announcement_emit_union(0, 0, members_four, 5);
    latest(&bundle->union_four);
    for (i = 0; i < 13; i++) members_long[i] = i;
    world_announcement_emit_union(0, 0, members_long, 13);
    latest(&bundle->union_long);
    stable &= bundle->union_long.alliance_a.valid && bundle->union_long.alliance_a.color == civs[0].color;
    bundle->union_stable = stable;
    bundle->union_duplicate_count = game_notifications_count() - notifications;
    return bundle->collapse.related_count == 5 && bundle->union_long.related_count == 12 &&
           bundle->union_four.related_count == 4 && stable && game_notifications_count() == notifications;
}

static int case_vassal_alliance(FILE *summary, WorldAnnouncementProbeBundle *bundle) {
    WorldAnnouncementEvent created, dissolved, joined, removed;
    int ok, vassal_ok;
    event_log_clear();
    world_announcement_emit_vassal(EVENT_TYPE_VASSAL_INDEPENDENCE_WAR, 6, 7, -1, 0);
    latest(&bundle->vassal_independence);
    vassal_ok = bundle->vassal_independence.priority == WORLD_ANNOUNCEMENT_CRITICAL &&
                 bundle->vassal_independence.actor.color == civs[6].color &&
                 bundle->vassal_independence.target.color == civs[7].color;
    fprintf(summary,
        "case=world_announcement_vassal_independence ok=%d vassal_color=%d overlord_color=%d\n",
        vassal_ok,
        bundle->vassal_independence.actor.color == civs[6].color,
        bundle->vassal_independence.target.color == civs[7].color);
    event_log_clear();
    world_announcement_emit_alliance_created(0, 0, 2); latest(&created);
    world_announcement_emit_alliance_dissolved(0, 0); latest(&dissolved);
    world_announcement_emit_alliance_member_joined(0, 0, 3); latest(&joined);
    world_announcement_emit_alliance_member_removed(0, 3, 0); latest(&removed);
    world_announcement_emit_alliance_military_changed(0, 1); latest(&bundle->military_upgrade);
    world_announcement_emit_alliance_military_changed(0, 0); latest(&bundle->military_downgrade);
    ok = created.priority == WORLD_ANNOUNCEMENT_MAJOR && dissolved.priority == WORLD_ANNOUNCEMENT_MAJOR &&
         joined.priority == WORLD_ANNOUNCEMENT_NORMAL && removed.priority == WORLD_ANNOUNCEMENT_NORMAL &&
         bundle->military_upgrade.priority == WORLD_ANNOUNCEMENT_CRITICAL &&
         bundle->military_downgrade.priority == WORLD_ANNOUNCEMENT_CRITICAL;
    fprintf(summary,
        "case=world_announcement_alliance_policy ok=%d create_major=%d dissolve_major=%d join_normal=%d remove_normal=%d\n",
        ok, created.priority == 2, dissolved.priority == 2, joined.priority == 1, removed.priority == 1);
    return ok && vassal_ok;
}

static int case_plague(FILE *summary, WorldAnnouncementProbeBundle *bundle) {
    PlagueState *states = calloc(MAX_CITIES, sizeof(*states));
    int start, spread, end;
    if (!states) return 0;
    event_log_clear();
    world_announcement_state_reset();
    plague_restore_save_state(states, MAX_CITIES, NULL, 0, -1);
    world_announcement_plague_observe();
    states[0].active = 1; states[0].infected = 1; states[0].severity = 4; states[0].months_left = 12;
    plague_restore_save_state(states, MAX_CITIES, NULL, 0, 0);
    world_announcement_plague_observe();
    start = world_announcement_store_count() == 1 && latest(&bundle->plague_started);
    event_log_push_structured(EVENT_TYPE_PLAGUE_SPREAD, EVENT_SEVERITY_INFO, 0, -1, -1, 0, 0, 0, "");
    world_announcement_plague_observe();
    spread = world_announcement_store_count() == 1;
    memset(states, 0, sizeof(*states) * MAX_CITIES);
    plague_restore_save_state(states, MAX_CITIES, NULL, 0, -1);
    world_announcement_plague_observe();
    end = world_announcement_store_count() == 2 && latest(&bundle->plague_ended);
    fprintf(summary,
        "case=world_announcement_plague_policy ok=%d global_start=%d spread_suppressed=%d global_end=%d\n",
        start && spread && end, start, spread, end);
    free(states);
    return start && spread && end;
}

static int case_war(FILE *summary, WorldAnnouncementProbeBundle *bundle,
                    AllianceSaveState *work) {
    WorldAnnouncementEvent alliance_country;
    int count, ava, avc, suppressed, victory, one_terminal, truce;
    event_log_clear();
    install_alliance_fixture(work);
    world_announcement_war_reset();
    world_announcement_war_started(0, 0, 1);
    ava = latest(&bundle->alliance_war_started) &&
          bundle->alliance_war_started.war_class == WORLD_ANNOUNCEMENT_WAR_ALLIANCE_VS_ALLIANCE;
    world_announcement_war_ended(0, WAR_OUTCOME_ATTACKER_WIN, DIP_LAST_WAR_NEGOTIATED_TRUCE);
    victory = latest(&bundle->alliance_war_victory) &&
              bundle->alliance_war_victory.terminal_result == WORLD_ANNOUNCEMENT_TERMINAL_VICTORY;
    count = world_announcement_store_count();
    world_announcement_war_ended(0, WAR_OUTCOME_STALEMATE, DIP_LAST_WAR_NEGOTIATED_TRUCE);
    one_terminal = world_announcement_store_count() == count;
    world_announcement_war_started(1, 2, 3);
    avc = latest(&alliance_country) &&
          alliance_country.war_class == WORLD_ANNOUNCEMENT_WAR_ALLIANCE_VS_COUNTRY;
    world_announcement_war_ended(1, WAR_OUTCOME_STALEMATE, DIP_LAST_WAR_NEGOTIATED_TRUCE);
    truce = latest(&bundle->alliance_war_truce) &&
            bundle->alliance_war_truce.terminal_result == WORLD_ANNOUNCEMENT_TERMINAL_NEGOTIATED_TRUCE;
    count = world_announcement_store_count();
    world_announcement_war_started(2, 4, 5);
    world_announcement_war_ended(2, WAR_OUTCOME_STALEMATE, DIP_LAST_WAR_NEGOTIATED_TRUCE);
    suppressed = world_announcement_store_count() == count;
    fprintf(summary,
        "case=world_announcement_alliance_war ok=%d alliance_vs_alliance=%d alliance_vs_country=%d country_vs_country_suppressed=%d\n",
        ava && avc && suppressed, ava, avc, suppressed);
    fprintf(summary,
        "case=world_announcement_war_terminal ok=%d victory=%d duplicate_truce_suppressed=%d negotiated_truce=%d one_terminal=%d\n",
        victory && one_terminal && truce, victory, one_terminal, truce, one_terminal);
    return ava && avc && suppressed && victory && one_terminal && truce;
}

int game_presentation_world_announcement_event_probe(
    FILE *summary, WorldAnnouncementProbeBundle *bundle) {
    AnnouncementFixtureBackup backup;
    AllianceSaveState *work = malloc(sizeof(*work));
    int ok;
    if (!bundle || !work) { free(work); return 0; }
    memset(bundle, 0, sizeof(*bundle));
    if (!fixture_begin(&backup, work)) { free(work); return 0; }
    ok = case_age(summary, bundle);
    ok &= case_collapse_union(bundle, work);
    ok &= case_vassal_alliance(summary, bundle);
    ok &= case_plague(summary, bundle);
    ok &= case_war(summary, bundle, work);
    fixture_end(&backup);
    free(work);
    return ok;
}

int game_presentation_world_announcement_probe(FILE *summary) {
    WorldAnnouncementProbeBundle *bundle = calloc(1, sizeof(*bundle));
    int ok;
    if (!bundle) return 0;
    ok = game_presentation_world_announcement_event_probe(summary, bundle);
    ok &= game_presentation_world_announcement_layout_probe(summary, bundle);
    ok &= game_presentation_world_announcement_ui_probe(summary, bundle);
    free(bundle);
    return ok;
}
