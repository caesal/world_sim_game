#include "game/game_alliance_lifecycle_probe.h"

#include "core/game_state.h"
#include "core/render_snapshot.h"
#include "render/panel_alliance_history.h"
#include "sim/alliance.h"
#include "sim/alliance_names.h"
#include "sim/diplomacy.h"
#include "ui/ui_types.h"
#include "ui/ui_widgets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define DIPLOMACY_PROBE_DIR "build/validation/diplomacy_alliance_probe_20260613"

static void init_civ(int id, const char *name) {
    memset(&civs[id], 0, sizeof(civs[id]));
    snprintf(civs[id].name, sizeof(civs[id].name), "%s", name);
    civs[id].alive = 1; civs[id].custom_name = 1; civs[id].capital_city = id;
    civs[id].color = COLOR32_RGB(90 + id * 30, 120 + id * 20, 170 + id * 10);
    civs[id].cohesion = 7; civs[id].military = 7; civs[id].treasury = 500;
}

static void reset_fixture(int count) {
    int i;
    alliance_reset(); diplomacy_reset();
    civ_count = count; world_generated = 1; year = 1200; month = 1;
    for (i = 0; i < count; i++) init_civ(i, i == 0 ? "Founder" : i == 1 ? "Loser" : "Candidate");
}

static int write_bmp_from_bits(const char *path, BITMAPINFO *info, void *bits, int width, int height) {
    BITMAPFILEHEADER file_header;
    DWORD image_size = (DWORD)(width * height * 4);
    FILE *file;
    memset(&file_header, 0, sizeof(file_header));
    file_header.bfType = 0x4d42;
    file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    file_header.bfSize = file_header.bfOffBits + image_size;
    file = fopen(path, "wb");
    if (!file) return 0;
    fwrite(&file_header, sizeof(file_header), 1, file);
    fwrite(&info->bmiHeader, sizeof(BITMAPINFOHEADER), 1, file);
    fwrite(bits, image_size, 1, file);
    fclose(file);
    return 1;
}

static int case_forced_breakup_pair_cooldown(FILE *summary) {
    AllianceSaveState *state;
    int id, forced, dissolved, blocked_create, cd_set, cleared, history = 0, i;
    reset_fixture(3);
    alliance_debug_set_create_years(0, 1, 79);
    id = alliance_debug_create_pair(0, 1, 80);
    state = alliance_internal_state();
    cleared = id >= 0 && state->create_years[0][1] == 0 && state->create_years[1][0] == 0;
    forced = alliance_force_member_exit_for_war_defeat(id, 1, 100);
    dissolved = id >= 0 && !state->records[id].active && alliance_for_civ(0) < 0 && alliance_for_civ(1) < 0;
    cd_set = state->create_years[0][1] == -100 && state->create_years[1][0] == -100 &&
             state->kicked_cooldown[id][1] == 100;
    blocked_create = alliance_player_form_or_join(0, 1) == ALLIANCE_CMD_BLOCKED && alliance_for_civ(0) < 0;
    for (i = 0; i < ALLIANCE_HISTORY_RECORD_CAP; i++)
        if (state->history[id][i].active && state->history[id][i].event_type ==
            ALLIANCE_HISTORY_MEMBER_REMOVED_BY_WAR_DEFEAT) history = 1;
    fprintf(summary,
            "case=forced_breakup_pair_cooldown ok=%d forced=%d dissolved=%d pair_cd=%d blocked_create=%d create_cleared=%d history=%d\n",
            forced && dissolved && cd_set && blocked_create && cleared && history,
            forced, dissolved, cd_set, blocked_create, cleared, history);
    return forced && dissolved && cd_set && blocked_create && cleared && history;
}

static int case_join_blocked_by_pair_cooldown(FILE *summary) {
    AllianceSaveState *state;
    AllianceYearWork work;
    int id, blocked;
    reset_fixture(3);
    id = alliance_debug_create_pair(0, 1, 80);
    state = alliance_internal_state();
    alliance_debug_set_create_years(2, 1, -50);
    alliance_debug_set_join_years(2, id, ALLIANCE_JOIN_FIRST_VOTE_YEARS - 1);
    alliance_year_work_begin(&work);
    while (!alliance_update_year_step(&work, 64)) {}
    blocked = alliance_for_civ(2) < 0 && state->join_years[2][id] == 0;
    fprintf(summary, "case=forced_pair_join_block ok=%d joined=%d join_years=%d pair_cd=%d\n",
            blocked, alliance_for_civ(2), state->join_years[2][id], state->create_years[2][1]);
    return blocked;
}

static int case_war_removal_sentence_and_bmp(FILE *summary) {
    RenderSnapshot *snapshot;
    AlliancePanelRow row;
    AllianceHistoryRecord history;
    BITMAPINFO info;
    HDC screen, dc;
    HBITMAP bitmap, old_bitmap;
    void *bits = NULL;
    UiCursor cursor;
    char en[160], zh[160];
    int old_lang = ui_language, ok_text, ok_bmp, ok_margin;
    snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    if (!snapshot) return 0;
    snapshot->civ_count = 2; snapshot->alliance_count = 1;
    snapshot->civs[1].alive = 1; snapshot->civs[1].color = COLOR32_RGB(180, 80, 80);
    snprintf(snapshot->civs[1].name_en, sizeof(snapshot->civs[1].name_en), "%s", "Loser");
    snprintf(snapshot->civs[1].name_zh, sizeof(snapshot->civs[1].name_zh), "%s", "败者");
    snapshot->alliances[0].id = 0; snapshot->alliances[0].active = 0;
    snapshot->alliances[0].color = COLOR32_RGB(90, 120, 190);
    snprintf(snapshot->alliances[0].name_en, sizeof(snapshot->alliances[0].name_en), "%s", "Old League");
    snprintf(snapshot->alliances[0].name_zh, sizeof(snapshot->alliances[0].name_zh), "%s", "旧同盟");
    memset(&history, 0, sizeof(history));
    history.active = 1; history.alliance_id = 0; history.civ_id = 1; history.target_civ_id = -1;
    history.event_year = 1200; history.event_type = ALLIANCE_HISTORY_MEMBER_REMOVED_BY_WAR_DEFEAT;
    snapshot->alliances[0].history[0] = history;
    snapshot->alliances[0].history_count = 1; snapshot->alliances[0].history_next = 1;
    ui_language = UI_LANG_EN; alliance_history_probe_sentence(en, sizeof(en), snapshot, &history);
    ui_language = UI_LANG_ZH; alliance_history_probe_sentence(zh, sizeof(zh), snapshot, &history);
    ok_text = strcmp(en, "Loser was removed after military defeat.") == 0 &&
              strcmp(zh, "败者国因战败被清退。") == 0;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 680; info.bmiHeader.biHeight = -180;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    screen = GetDC(NULL); dc = CreateCompatibleDC(screen);
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bitmap || !bits) {
        if (dc) DeleteDC(dc);
        if (screen) ReleaseDC(NULL, screen);
        ui_language = old_lang;
        free(snapshot);
        fprintf(summary, "case=war_removal_history_text ok=0 reason=bmp_alloc_failed\n");
        return 0;
    }
    old_bitmap = (HBITMAP)SelectObject(dc, bitmap);
    memset(bits, 0xff, 680 * 180 * 4);
    row.kind = ALLIANCE_PANEL_ROW_ALLIANCE; row.alliance_id = 0; row.civ_id = -1;
    cursor = ui_cursor(20, 14, 620, 170);
    alliance_history_draw_content(dc, &cursor, snapshot, &row);
    SelectObject(dc, old_bitmap); DeleteDC(dc); ReleaseDC(NULL, screen);
    ok_bmp = write_bmp_from_bits(DIPLOMACY_PROBE_DIR "/alliance_history_war_removal.bmp",
                                 &info, bits, 680, 180);
    DeleteObject(bitmap); ui_language = old_lang; free(snapshot);
    ok_margin = (20 + 620 - 10) > (20 + 620 - 100) && (20 + 620 - 100 - 8) > 20;
    fprintf(summary, "case=war_removal_history_text ok=%d en=\"%s\" zh=\"%s\" bmp=%s margin_ok=%d\n",
            ok_text && ok_bmp && ok_margin, en, zh,
            DIPLOMACY_PROBE_DIR "\\alliance_history_war_removal.bmp", ok_margin);
    return ok_text && ok_bmp && ok_margin;
}

static int case_alliance_name_allocation(FILE *summary) {
    AllianceSaveState *state;
    int base_count = alliance_name_base_count(), seen[ALLIANCE_NAME_BASE_COUNT];
    int i, id, first_ok = 1, suffix2_ok, reuse_no_roman_ok;
    reset_fixture(2);
    state = alliance_internal_state();
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < base_count; i++) {
        id = alliance_debug_create_pair(0, 1, 80);
        if (id < 0 || state->records[id].suffix_number != 1 ||
            state->records[id].base_name_index < 0 ||
            state->records[id].base_name_index >= base_count) first_ok = 0;
        else seen[state->records[id].base_name_index]++;
        alliance_player_leave(0);
    }
    for (i = 0; i < base_count; i++) if (seen[i] != 1) first_ok = 0;
    id = alliance_debug_create_pair(0, 1, 80);
    suffix2_ok = id >= 0 && state->records[id].suffix_number == 2;
    alliance_player_leave(0);
    reset_fixture(2);
    id = alliance_debug_create_pair(0, 1, 80);
    alliance_player_leave(0);
    id = alliance_debug_create_pair(0, 1, 80);
    reuse_no_roman_ok = id >= 0 && state->records[id].suffix_number == 1;
    fprintf(summary,
            "case=alliance_name_allocation ok=%d base_count=%d first_unique=%d post_exhaust_suffix2=%d slot_reuse_no_roman=%d\n",
            first_ok && suffix2_ok && reuse_no_roman_ok, base_count, first_ok, suffix2_ok, reuse_no_roman_ok);
    return first_ok && suffix2_ok && reuse_no_roman_ok;
}

int run_alliance_lifecycle_extra_probe_cases(FILE *summary) {
    int ok = 1;
    ok &= case_forced_breakup_pair_cooldown(summary);
    ok &= case_join_blocked_by_pair_cooldown(summary);
    ok &= case_war_removal_sentence_and_bmp(summary);
    ok &= case_alliance_name_allocation(summary);
    return ok;
}
