#include "game/game_alliance_probe.h"

#include "game/game_alliance_record_probe.h"
#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "core/render_snapshot.h"
#include "game/game_alliance_render_probe.h"
#include "game/game_player_actions.h"
#include "game/game_worldgen.h"
#include "render/map_label_alliance.h"
#include "render/map_label_cache.h"
#include "render/map_label_style.h"
#include "render/map_display_policy.h"
#include "render/render_context.h"
#include "render/render_static_map_cache.h"
#include "render/render_static_scene.h"
#include "sim/alliance.h"
#include "sim/civilization_slots.h"
#include "sim/diplomacy.h"
#include "sim/diplomacy_borders.h"
#include "sim/maritime.h"
#include "sim/population.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/stability_decision.h"
#include "sim/vassal.h"
#include "sim/war.h"
#include "sim/war_desire.h"
#include "ui/ui_types.h"
#include "world/terrain_query.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define ALLIANCE_PROBE_DIR "build/validation/diplomacy_alliance_probe_20260613"

static void init_civ(int id, int city_id, int military, Color32 color) {
    civilization_reset_slot_state(id);
    snprintf(civs[id].name, sizeof(civs[id].name), "Alliance Probe %d", id);
    civs[id].custom_name = 1; civs[id].alive = 1; civs[id].heritage = CIV_HERITAGE_WESTERN;
    civs[id].capital_city = city_id; civs[id].military = military; civs[id].governance = 8;
    civs[id].cohesion = 8; civs[id].commerce = 6; civs[id].logistics = 6; civs[id].color = color;
    civs[id].treasury = 900; civs[id].treasury_cap = 1500;
}

static void init_region_city(int id, int owner, int x, int pop) {
    NaturalRegion *region = &natural_regions[id];
    City *city = &cities[id];
    memset(region, 0, sizeof(*region)); memset(city, 0, sizeof(*city));
    region->id = id; region->alive = 1; region->owner_civ = owner; region->city_id = id;
    region->tile_count = 8; region->center_x = x; region->center_y = 3;
    region->capital_x = x; region->capital_y = 3; region->habitability = 7;
    region->development_score = 60; region->average_stats.food = 7;
    region->average_stats.water = 7; region->average_stats.pop_capacity = 7;
    region->total_stats = region->average_stats;
    city->alive = 1; city->owner = owner; city->x = x; city->y = 3; city->radius = 1;
    city->capital = 1; snprintf(city->name, sizeof(city->name), "Probe City %d", id);
    world[3][x].geography = GEO_PLAIN; world[3][x].climate = CLIMATE_CONTINENTAL;
    world[3][x].owner = owner; world[3][x].province_id = id; world[3][x].region_id = id;
    population_init_city(id, pop);
}

static void claim_tile(int owner, int region_id, int x, int y) {
    world[y][x].geography = GEO_PLAIN; world[y][x].climate = CLIMATE_CONTINENTAL;
    world[y][x].owner = owner; world[y][x].province_id = region_id; world[y][x].region_id = region_id;
    if (region_id >= 0 && region_id < region_count) natural_regions[region_id].tile_count++;
}

static void add_all_neighbors(int count) {
    int a, b;
    for (a = 0; a < count; a++) {
        for (b = a + 1; b < count; b++) {
            if (natural_regions[a].neighbor_count < MAX_REGION_NEIGHBORS)
                natural_regions[a].neighbors[natural_regions[a].neighbor_count++] = b;
            if (natural_regions[b].neighbor_count < MAX_REGION_NEIGHBORS)
                natural_regions[b].neighbors[natural_regions[b].neighbor_count++] = a;
        }
    }
}

static DiplomacyRelation relation_with(int state, int score) {
    DiplomacyRelation relation;
    memset(&relation, 0, sizeof(relation));
    relation.state = state; relation.relation_score = score; relation.years_known = 120;
    relation.contact_kind = DIP_CONTACT_LAND_BORDER; relation.overlord = -1; relation.vassal = -1;
    relation.last_war_winner = -1; relation.last_war_loser = -1;
    relation.last_war_result = DIP_LAST_WAR_NONE;
    return relation;
}

static void set_pair(int a, int b, int ab_score, int ba_score, int state) {
    diplomacy_restore_relation(a, b, relation_with(state, ab_score));
    diplomacy_restore_relation(b, a, relation_with(state, ba_score));
}

static void reset_fixture(int count) {
    int i, j;
    set_active_map_size(MAP_SIZE_SMALL);
    diplomacy_reset(); war_reset(); war_desire_reset_all(); stability_decision_reset();
    vassal_normalize_all(); simulation_reset_state(); alliance_reset(); game_clear_world_tiles();
    memset(natural_regions, 0, sizeof(natural_regions)); memset(cities, 0, sizeof(cities));
    civ_count = count; city_count = count; region_count = count; world_generated = 1; year = 500;
    for (i = 0; i < count; i++) {
        init_civ(i, i, i == 0 ? 10 : i == 5 ? 12 : 5, COLOR32_RGB(80 + i * 23, 118 + i * 11, 150 + i * 7));
        init_region_city(i, i, 2 + i, i == 5 ? 9000 : 3500);
    }
    if (count > 2) { claim_tile(2, 2, 2, 4); claim_tile(2, 2, 3, 4); }
    if (count > 3) claim_tile(3, 3, 4, 4);
    if (count > 4) claim_tile(4, 4, 5, 4);
    if (count > 5) claim_tile(5, 5, 3, 5);
    add_all_neighbors(count);
    terrain_stats_invalidate_cache(); world_recalculate_territory(); population_sync_all();
    maritime_rebuild_routes(); diplomacy_borders_mark_dirty(); diplomacy_mark_contacts_dirty();
    diplomacy_update_contacts();
    for (i = 0; i < count; i++) for (j = i + 1; j < count; j++) set_pair(i, j, 100, 100, DIPLOMACY_PEACE);
}

static int case_name_allocation(FILE *summary) {
    AllianceSaveState *state;
    int first, reused, no_suffix, roman_suffix;
    reset_fixture(4);
    first = alliance_debug_create_pair(0, 1, 80);
    no_suffix = first >= 0 && strstr(alliance_name_en(first), " II") == NULL &&
                strstr(alliance_name_zh(first), " II") == NULL;
    reset_fixture(4);
    state = alliance_internal_state();
    { int i; for (i = 0; i < 256; i++) state->name_use_count[i] = 1; }
    reused = alliance_debug_create_pair(0, 1, 80);
    roman_suffix = reused >= 0 && strstr(alliance_name_en(reused), " II") &&
                   strstr(alliance_name_zh(reused), " II");
    fprintf(summary, "case=alliance_name_allocation first=%d no_suffix=%d reused=%d roman_II=%d en=\"%s\"\n",
            first, no_suffix, reused, roman_suffix, reused >= 0 ? alliance_name_en(reused) : "");
    return no_suffix && roman_suffix;
}

static int case_alliance_slot_reuse(FILE *summary) {
    AllianceSaveState *s; int first, reused, high, exhaust, clean = 0, i;
    reset_fixture(4); first = alliance_debug_create_pair(0, 1, 80); alliance_player_leave(0); reused = alliance_debug_create_pair(2, 3, 80);
    reset_fixture(4); s = alliance_internal_state(); s->next_id = ALLIANCE_MAX;
    for (i = 0; i < ALLIANCE_MAX; i++) { s->records[i].active = 1; s->records[i].id = i; }
    s->records[5].active = 0; s->join_years[0][5] = 44; s->kick_years[5][1] = 55; s->voluntary_cooldown[5][2] = 66; s->kicked_cooldown[5][3] = 77; s->candidate_count[5] = 3; s->vote_count[5] = 4; s->history_count[5] = 5;
    high = alliance_debug_create_pair(0, 1, 80);
    clean = high == 5 && s->records[5].active && s->records[5].member_count == 2 && s->candidate_count[5] == 0 && s->vote_count[5] == 0 && s->history_count[5] == 1 && !s->join_years[0][5] && !s->kick_years[5][1] && !s->voluntary_cooldown[5][2] && !s->kicked_cooldown[5][3];
    reset_fixture(4); s = alliance_internal_state(); s->next_id = ALLIANCE_MAX; for (i = 0; i < ALLIANCE_MAX; i++) { s->records[i].active = 1; s->records[i].id = i; }
    exhaust = alliance_debug_create_pair(0, 1, 80);
    fprintf(summary, "case=alliance_slot_reuse first=%d reused=%d high=%d exhaust=%d clean=%d next_id=%d\n", first, reused, high, exhaust, clean, s->next_id);
    return first == 0 && reused == 0 && high == 5 && exhaust < 0 && clean;
}
static int case_player_actions(FILE *summary) {
    GamePlayerActionResult create, join, diff, vassal_block, leave;
    int alliance_a, alliance_b, member_count, vassal_display, vassal_formal, founder_after_leave;
    reset_fixture(6);
    create = game_player_form_alliance(0, 1);
    alliance_a = alliance_for_civ(0);
    join = game_player_form_alliance(1, 2);
    member_count = alliance_member_count(alliance_a);
    game_player_form_alliance(3, 4);
    diff = game_player_form_alliance(2, 3);
    leave = game_player_leave_alliance(0);
    alliance_b = alliance_for_civ(1);
    founder_after_leave = alliance_founder(alliance_b);
    reset_fixture(3);
    game_player_form_alliance(0, 1);
    alliance_a = alliance_for_civ(0);
    vassal_make(1, 2, 80);
    vassal_normalize_all();
    vassal_block = game_player_form_alliance(1, 2);
    vassal_display = alliance_display_for_civ(2);
    vassal_formal = alliance_for_civ(2);
    fprintf(summary,
            "case=alliance_player_actions create=%d join=%d count=%d diff=%d vassal=%d display=%d formal=%d leave=%d new_founder=%d\n",
            create, join, member_count, diff, vassal_block, vassal_display, vassal_formal, leave,
            founder_after_leave);
    return create == GAME_PLAYER_ACTION_OK && join == GAME_PLAYER_ACTION_OK && member_count == 3 &&
           diff == GAME_PLAYER_ACTION_DIFFERENT_ALLIANCES &&
           vassal_block == GAME_PLAYER_ACTION_VASSAL_ALLIANCE_BLOCKED &&
           vassal_display == alliance_a && vassal_formal < 0 &&
           leave == GAME_PLAYER_ACTION_OK && founder_after_leave == 1;
}

static int case_ai_lifecycle(FILE *summary) {
    int created = -1, joined = 0, kicked = 0, seed, create_seed = -1, join_seed = -1;
    reset_fixture(5);
    set_pair(0, 1, 94, 100, DIPLOMACY_PEACE);
    alliance_debug_set_create_years(0, 1, 79);
    alliance_update_year();
    if (alliance_for_civ(0) >= 0) return 0;
    set_pair(0, 1, 100, 100, DIPLOMACY_PEACE);
    for (seed = 0; seed < 200 && created < 0; seed++) {
        reset_fixture(5); set_pair(0, 1, 100, 100, DIPLOMACY_PEACE);
        alliance_debug_set_create_years(0, 1, 79); srand((unsigned int)seed); alliance_update_year();
        if (alliance_for_civ(0) >= 0) { created = alliance_for_civ(0); create_seed = seed; }
    }
    for (seed = 0; seed < 200 && !joined; seed++) {
        reset_fixture(5); created = alliance_debug_create_pair(0, 1, 80);
        set_pair(2, 0, 100, 100, DIPLOMACY_PEACE); set_pair(2, 1, 100, 100, DIPLOMACY_PEACE);
        alliance_debug_set_join_years(2, created, ALLIANCE_JOIN_FIRST_VOTE_YEARS - 1);
        srand((unsigned int)seed); alliance_update_year();
        joined = alliance_for_civ(2) == created; if (joined) join_seed = seed;
    }
    reset_fixture(5); created = alliance_debug_create_pair(0, 1, 80); alliance_debug_add_member(created, 2, 80);
    set_pair(0, 2, -10, 100, DIPLOMACY_ALLIANCE); set_pair(1, 2, -10, 100, DIPLOMACY_ALLIANCE);
    alliance_debug_set_kick_years(created, 2, ALLIANCE_REMOVAL_FIRST_VOTE_YEARS - 1); alliance_update_year();
    kicked = alliance_for_civ(2) < 0 && alliance_internal_state()->kicked_cooldown[created][2] == 100;
    fprintf(summary,
            "case=alliance_ai_lifecycle create_seed=%d created=%d join_seed=%d joined=%d kicked=%d cooldown=%d\n",
            create_seed, created >= 0, join_seed, joined, kicked,
            alliance_internal_state()->kicked_cooldown[created][2]);
    return create_seed >= 0 && join_seed >= 0 && kicked;
}

static int case_defensive_power(FILE *summary) {
    WarDesireBreakdown before, after;
    int id, own, bloc, bloc_again, own_recompute, bloc_recompute;
    reset_fixture(6);
    set_pair(5, 1, -80, -80, DIPLOMACY_TENSE);
    before = war_desire_calculate(5, 1, diplomacy_relation(5, 1));
    id = alliance_debug_create_pair(0, 1, 80);
    alliance_power_cache_reset();
    own = alliance_own_power(1);
    bloc = alliance_defensive_bloc_power(1);
    bloc_again = alliance_defensive_bloc_power(1);
    own_recompute = alliance_power_own_recompute_count();
    bloc_recompute = alliance_power_bloc_recompute_count();
    after = war_desire_calculate(5, 1, diplomacy_relation(5, 1));
    fprintf(summary,
            "case=alliance_defensive_power id=%d own=%d bloc=%d again=%d own_recompute=%d bloc_recompute=%d before_strength=%d after_strength=%d before_final=%d after_final=%d\n",
            id, own, bloc, bloc_again, own_recompute, bloc_recompute,
            before.strength_score, after.strength_score,
            before.final_desire, after.final_desire);
    return id >= 0 && bloc > own && bloc_again == bloc && own_recompute == 2 &&
           bloc_recompute == 1 && after.strength_score < before.strength_score;
}

static int case_dirty_scope(FILE *summary) {
    int own_before, own_after, prov_before, prov_after, civ_visual_before, civ_visual_after;
    int alliance_before, alliance_after, diplomacy_before, diplomacy_after, stable_after;
    reset_fixture(4);
    dirty_reset_all();
    own_before = dirty_revision_ownership(); prov_before = dirty_revision_province();
    civ_visual_before = dirty_revision_civ_visual();
    alliance_before = dirty_revision_alliance(); diplomacy_before = dirty_revision_diplomacy();
    alliance_debug_create_pair(0, 1, 80);
    own_after = dirty_revision_ownership(); prov_after = dirty_revision_province();
    civ_visual_after = dirty_revision_civ_visual();
    alliance_after = dirty_revision_alliance(); diplomacy_after = dirty_revision_diplomacy();
    alliance_sanitize_loaded();
    stable_after = dirty_revision_alliance();
    fprintf(summary,
            "case=alliance_dirty_scope own=%d/%d prov=%d/%d civ_visual=%d/%d alliance=%d/%d stable=%d diplomacy=%d/%d labels_dirty=%d political_dirty=%d borders_dirty=%d\n",
            own_before, own_after, prov_before, prov_after, civ_visual_before, civ_visual_after,
            alliance_before, alliance_after, stable_after, diplomacy_before, diplomacy_after,
            dirty_render_labels(), dirty_render_political(), dirty_render_borders());
    return own_before == own_after && prov_before == prov_after &&
           civ_visual_before == civ_visual_after && alliance_after > alliance_before &&
           stable_after == alliance_after && diplomacy_after > diplomacy_before &&
           dirty_render_labels() && !dirty_render_political() && !dirty_render_borders();
}

static int write_label_bmp(const char *path, BITMAPINFO *info, void *bits, int width, int height) {
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

static COLORREF sample_dib_color(const void *bits, int width, int x, int y) {
    const unsigned int *pixels = (const unsigned int *)bits;
    unsigned int value = pixels[y * width + x];
    return RGB((value >> 16) & 255, (value >> 8) & 255, value & 255);
}

static void fill_static_snapshot(RenderSnapshot *snapshot);

static void fill_label_snapshot(RenderSnapshot *snapshot) {
    int x, y;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->world_generated = 1; snapshot->map_w = 96; snapshot->map_h = 54;
    snapshot->civ_count = 4; snapshot->alliance_count = 1;
    for (x = 0; x < 4; x++) {
        SnapshotCiv *civ = &snapshot->civs[x];
        civ->alive = 1; civ->id = x; civ->alliance_display_id = (x < 2 || x == 3) ? 0 : -1;
        civ->alliance_id = x < 2 ? 0 : -1; civ->overlord = x == 3 ? 0 : -1;
        civ->color = x == 2 ? COLOR32_RGB(214, 92, 74) : COLOR32_RGB(86, 132, 196);
        civ->summary.territory = 700;
        snprintf(civ->name_en, sizeof(civ->name_en), "%s", x == 2 ? "Independent" : "Member");
    }
    snapshot->alliances[0].active = 1; snapshot->alliances[0].id = 0; snapshot->alliances[0].member_count = 2;
    snapshot->alliances[0].color = COLOR32_RGB(86, 152, 218);
    snprintf(snapshot->alliances[0].name_en, sizeof(snapshot->alliances[0].name_en), "%s", "Blue Accord");
    for (y = 0; y < snapshot->map_h; y++) for (x = 0; x < snapshot->map_w; x++) {
        SnapshotTile *tile = &snapshot->tiles[y * snapshot->map_w + x];
        tile->owner = x < 24 ? 0 : x < 48 ? 1 : x < 72 ? 2 : 3;
        tile->geography = GEO_PLAIN; tile->province_id = (short)tile->owner; tile->region_id = (short)tile->owner;
    }
}

static int color_delta(COLORREF a, COLORREF b) {
    return abs(GetRValue(a) - GetRValue(b)) +
           abs(GetGValue(a) - GetGValue(b)) +
           abs(GetBValue(a) - GetBValue(b));
}

static int draw_static_until_safe(HDC hdc, RECT client, MapLayout layout,
                                  RenderSnapshot *snapshot, int mode) {
    int i;
    display_mode = mode;
    fill_static_snapshot(snapshot);
    render_context_begin(snapshot);
    for (i = 0; i < 10; i++) {
        draw_cached_static_map_nonblocking(hdc, client, layout);
        if (render_static_map_cache_presented_boundary_safe()) break;
    }
    render_context_end();
    return render_static_map_cache_presented_boundary_safe();
}

static void fill_static_snapshot(RenderSnapshot *snapshot) {
    fill_label_snapshot(snapshot);
    snapshot->terrain_revision = dirty_revision_terrain();
    snapshot->coast_revision = dirty_revision_coast();
    snapshot->hydrology_revision = dirty_revision_hydrology();
    snapshot->regions_revision = dirty_revision_ownership() ^ dirty_revision_province();
    snapshot->civ_visual_revision = dirty_revision_civ_visual();
    snapshot->alliance_revision = dirty_revision_alliance();
}

static int case_alliance_label_style(FILE *summary) {
    static RenderSnapshot snapshot;
    const int width = 640, height = 360;
    BITMAPINFO info; void *bits = NULL; HDC screen = GetDC(NULL), mem = CreateCompatibleDC(screen);
    HBITMAP bitmap = NULL; HGDIOBJ old_bitmap = NULL;
    RECT client = {0, 0, width, height};
    MapLayout layout = {0};
    int old_display = display_mode, old_language = ui_language, old_zoom = map_zoom_percent;
    int old_selected = selected_civ;
    MapLabelStyle normal, ordinary, alliance;
    int style_ok, file_ok = 0;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (bitmap && bits && mem) {
        HBRUSH bg = CreateSolidBrush(RGB(34, 45, 52));
        layout.map_x = 0; layout.map_y = 0; layout.tile_size = 4;
        layout.draw_w = width; layout.draw_h = height;
        old_bitmap = SelectObject(mem, bitmap);
        FillRect(mem, &client, bg); DeleteObject(bg);
        fill_label_snapshot(&snapshot);
        display_mode = DISPLAY_ALLIANCE; ui_language = UI_LANG_EN; map_zoom_percent = 120;
        selected_civ = -1; dirty_mark_labels();
        map_label_cache_draw_labels(mem, client, layout, &snapshot);
        file_ok = write_label_bmp(ALLIANCE_PROBE_DIR "/alliance_label_sample.bmp", &info, bits, width, height);
    }
    normal = map_label_style_for(LABEL_COUNTRY, 4, 0, 0);
    ordinary = normal; alliance = normal;
    map_label_alliance_apply_style(&ordinary, 2, 0);
    map_label_alliance_apply_style(&alliance, map_label_alliance_source_id(0), 0);
    style_ok = ordinary.text_color == normal.text_color && alliance.text_color == RGB(155, 215, 255);
    display_mode = old_display; ui_language = old_language; map_zoom_percent = old_zoom;
    selected_civ = old_selected;
    if (bitmap) { if (old_bitmap) SelectObject(mem, old_bitmap); DeleteObject(bitmap); }
    if (mem) DeleteDC(mem);
    if (screen) ReleaseDC(NULL, screen);
    fprintf(summary,
            "case=alliance_label_style ok=%d file=alliance_label_sample.bmp alliance_color=RGB(155,215,255) ordinary_color=RGB(245,225,170)\n",
            style_ok && file_ok);
    return style_ok && file_ok;
}

static int case_static_cache_stale_safe(FILE *summary) {
    static RenderSnapshot snapshot;
    const int width = 720, height = 420;
    BITMAPINFO info; void *bits = NULL; HDC screen = GetDC(NULL), mem = CreateCompatibleDC(screen);
    HBITMAP bitmap = NULL; HGDIOBJ old_bitmap = NULL;
    RECT client = {0, 0, width, height};
    RECT content = {0};
    MapLayout layout = {0};
    int old_display = display_mode, old_side_collapsed = side_panel_collapsed;
    int safe = 0, current = 1, complete = 0, missing_safe = 1, i;
    int alliance_color_ok = 0, independent_color_ok = 0, vassal_color_ok = 0;
    int policy_ok = 0, mode_switch_ok = 0, scene_direct_ok = 0;
    int file_ok = 0;
    COLORREF alliance_sample = RGB(0, 0, 0), independent_sample = RGB(0, 0, 0), vassal_sample = RGB(0, 0, 0);
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (bitmap && bits && mem) {
        old_bitmap = SelectObject(mem, bitmap);
        dirty_reset_all(); render_static_map_cache_reset_debug(); display_mode = DISPLAY_ALLIANCE;
        side_panel_collapsed = 1;
        fill_static_snapshot(&snapshot);
        content = get_map_content_rect(client);
        layout.map_x = content.left; layout.map_y = content.top; layout.tile_size = 4;
        layout.draw_w = snapshot.map_w * layout.tile_size;
        layout.draw_h = snapshot.map_h * layout.tile_size;
        {
            const SnapshotTile *member = &snapshot.tiles[(snapshot.map_h / 2) * snapshot.map_w + 12];
            const SnapshotTile *independent = &snapshot.tiles[(snapshot.map_h / 2) * snapshot.map_w + 60];
            const SnapshotTile *vassal = &snapshot.tiles[(snapshot.map_h / 2) * snapshot.map_w + 84];
            COLORREF member_base = map_display_policy_snapshot_base_color(member, DISPLAY_ALLIANCE);
            COLORREF member_alliance = map_display_policy_snapshot_tile_color(&snapshot, member, DISPLAY_ALLIANCE);
            COLORREF independent_alliance = map_display_policy_snapshot_tile_color(&snapshot, independent, DISPLAY_ALLIANCE);
            COLORREF vassal_alliance = map_display_policy_snapshot_tile_color(&snapshot, vassal, DISPLAY_ALLIANCE);
            COLORREF political = map_display_policy_snapshot_tile_color(&snapshot, independent, DISPLAY_POLITICAL);
            policy_ok = map_display_policy_requires_fill_layer(DISPLAY_POLITICAL) &&
                        map_display_policy_requires_fill_layer(DISPLAY_ALLIANCE) &&
                        color_delta(political, member_base) > 30 &&
                        color_delta(member_alliance, member_base) > 30 &&
                        color_delta(independent_alliance, member_base) > 20 &&
                        color_delta(vassal_alliance, member_alliance) < 8;
        }
        render_context_begin(&snapshot);
        draw_cached_static_map_nonblocking(mem, client, layout);
        missing_safe = render_static_map_cache_presented_boundary_safe();
        for (i = 0; i < 8; i++) draw_cached_static_map_nonblocking(mem, client, layout);
        complete = !render_static_map_cache_needs_work() &&
                   render_static_map_cache_presented_boundary_safe();
        alliance_sample = sample_dib_color(bits, width, layout.map_x + layout.draw_w / 8,
                                           layout.map_y + layout.draw_h / 2);
        independent_sample = sample_dib_color(bits, width, layout.map_x + layout.draw_w * 5 / 8,
                                              layout.map_y + layout.draw_h / 2);
        vassal_sample = sample_dib_color(bits, width, layout.map_x + layout.draw_w * 7 / 8,
                                         layout.map_y + layout.draw_h / 2);
        alliance_color_ok = GetBValue(alliance_sample) > GetRValue(alliance_sample) + 20 &&
                            GetGValue(alliance_sample) > GetRValue(alliance_sample);
        independent_color_ok = GetRValue(independent_sample) > GetBValue(independent_sample) + 20 &&
                               GetRValue(independent_sample) > 110;
        vassal_color_ok = color_delta(vassal_sample, alliance_sample) < 28;
        file_ok = write_label_bmp(ALLIANCE_PROBE_DIR "/alliance_static_fill_sample.bmp", &info, bits, width, height);
        render_context_end();
        mode_switch_ok = draw_static_until_safe(mem, client, layout, &snapshot, DISPLAY_POLITICAL) &&
                         draw_static_until_safe(mem, client, layout, &snapshot, DISPLAY_ALLIANCE) &&
                         draw_static_until_safe(mem, client, layout, &snapshot, DISPLAY_GEOGRAPHY) &&
                         draw_static_until_safe(mem, client, layout, &snapshot, DISPLAY_CLIMATE) &&
                         draw_static_until_safe(mem, client, layout, &snapshot, DISPLAY_ALLIANCE) &&
                         draw_static_until_safe(mem, client, layout, &snapshot, DISPLAY_POLITICAL);
        render_static_scene_invalidate_cache();
        render_static_map_cache_reset_debug();
        display_mode = DISPLAY_ALLIANCE;
        fill_static_snapshot(&snapshot);
        render_context_begin(&snapshot);
        render_static_scene_draw(mem, client, layout, &snapshot);
        scene_direct_ok = render_static_scene_complete();
        render_context_end();
        render_static_map_cache_reset_debug();
        draw_static_until_safe(mem, client, layout, &snapshot, DISPLAY_ALLIANCE);
        dirty_mark_alliance();
        fill_static_snapshot(&snapshot);
        render_context_begin(&snapshot);
        draw_cached_static_map_nonblocking(mem, client, layout);
        safe = render_static_map_cache_presented_boundary_safe();
        current = render_static_map_cache_presented_current();
        render_context_end();
    }
    display_mode = old_display; side_panel_collapsed = old_side_collapsed;
    if (bitmap) { if (old_bitmap) SelectObject(mem, old_bitmap); DeleteObject(bitmap); }
    if (mem) DeleteDC(mem);
    if (screen) ReleaseDC(NULL, screen);
    fprintf(summary,
            "case=alliance_static_cache_stale_safe missing_safe=%d complete=%d safe=%d current=%d alliance_rgb=%d,%d,%d independent_rgb=%d,%d,%d vassal_rgb=%d,%d,%d color_ok=%d/%d/%d policy=%d mode_switch=%d scene_direct=%d file=alliance_static_fill_sample.bmp\n",
            missing_safe, complete, safe, current,
            GetRValue(alliance_sample), GetGValue(alliance_sample), GetBValue(alliance_sample),
            GetRValue(independent_sample), GetGValue(independent_sample), GetBValue(independent_sample),
            GetRValue(vassal_sample), GetGValue(vassal_sample), GetBValue(vassal_sample),
            alliance_color_ok, independent_color_ok, vassal_color_ok,
            policy_ok, mode_switch_ok, scene_direct_ok);
    return !missing_safe && complete && safe && alliance_color_ok && independent_color_ok &&
           vassal_color_ok && policy_ok && mode_switch_ok && !scene_direct_ok && file_ok;
}

int run_alliance_probe_cases(FILE *summary) {
    int ok = 1;
    ok &= case_name_allocation(summary);
    ok &= case_alliance_slot_reuse(summary);
    ok &= case_player_actions(summary);
    ok &= case_ai_lifecycle(summary);
    ok &= case_defensive_power(summary);
    ok &= case_dirty_scope(summary);
    ok &= case_alliance_label_style(summary);
    ok &= case_static_cache_stale_safe(summary);
    ok &= run_alliance_record_probe_cases(summary);
    ok &= run_alliance_render_probe_cases(summary);
    return ok;
}
