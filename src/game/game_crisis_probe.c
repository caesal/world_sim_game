#include "game/game.h"

#include "core/game_state.h"
#include "core/render_snapshot.h"
#include "core/render_snapshot_cache.h"
#include "game/game_worldgen.h"
#include "render/panel_country_population.h"
#include "render/render_context.h"
#include "sim/civilization_slots.h"
#include "sim/decision_snapshot.h"
#include "sim/diplomacy.h"
#include "sim/diplomacy_borders.h"
#include "sim/maritime.h"
#include "sim/plague.h"
#include "sim/population.h"
#include "sim/population_diagnostics.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/stability_decision.h"
#include "sim/war.h"
#include "sim/war_desire.h"
#include "ui/ui_widgets.h"
#include "world/terrain_query.h"

#include <stdio.h>
#include <string.h>

#define CRISIS_PROBE_DIR "build/validation/crisis_war_population_20260607"

static DWORD last_population_render_ms = 0;

static void ensure_probe_dirs(void) {
    CreateDirectoryA("build", NULL);
    CreateDirectoryA("build/validation", NULL);
    CreateDirectoryA(CRISIS_PROBE_DIR, NULL);
}

static void add_region_neighbor(int a, int b) {
    NaturalRegion *ra;
    NaturalRegion *rb;
    if (a < 0 || a >= region_count || b < 0 || b >= region_count) return;
    ra = &natural_regions[a];
    rb = &natural_regions[b];
    if (ra->neighbor_count < MAX_REGION_NEIGHBORS) ra->neighbors[ra->neighbor_count++] = b;
    if (rb->neighbor_count < MAX_REGION_NEIGHBORS) rb->neighbors[rb->neighbor_count++] = a;
}

static void init_region(int id, int owner, int x, int y, int city_id) {
    NaturalRegion *region = &natural_regions[id];
    memset(region, 0, sizeof(*region));
    region->id = id;
    region->alive = 1;
    region->tile_count = 1;
    region->owner_civ = owner;
    region->city_id = city_id;
    region->center_x = x;
    region->center_y = y;
    region->capital_x = x;
    region->capital_y = y;
    region->habitability = 5;
    region->development_score = 30;
    region->cradle_score = 10;
    region->viable_direction_count = 2;
    region->average_stats.food = 4;
    region->average_stats.water = 4;
    region->average_stats.pop_capacity = 3;
    region->average_stats.habitability = 4;
    region->total_stats = region->average_stats;
    world[y][x].geography = GEO_PLAIN;
    world[y][x].climate = CLIMATE_CONTINENTAL;
    world[y][x].ecology = ECO_GRASSLAND;
    world[y][x].resource = RESOURCE_FEATURE_NONE;
    world[y][x].owner = owner;
    world[y][x].province_id = id;
    world[y][x].region_id = id;
}

static void init_civ(int id, const char *name, int city_id) {
    civilization_reset_slot_state(id);
    snprintf(civs[id].name, sizeof(civs[id].name), "%s", name);
    civs[id].custom_name = 1;
    civs[id].alive = 1;
    civs[id].heritage = CIV_HERITAGE_WESTERN;
    civs[id].aggression = 4;
    civs[id].expansion = 5;
    civs[id].governance = 7;
    civs[id].cohesion = 7;
    civs[id].production = 6;
    civs[id].military = 8;
    civs[id].commerce = 4;
    civs[id].logistics = 6;
    civs[id].innovation = 5;
    civs[id].capital_city = city_id;
    civs[id].treasury_cap = 1200;
}

static void init_city(int id, int owner, const char *name, int x, int y, int pop, int port) {
    City *city = &cities[id];
    memset(city, 0, sizeof(*city));
    city->alive = 1;
    city->owner = owner;
    snprintf(city->name, sizeof(city->name), "%s", name);
    city->x = x;
    city->y = y;
    city->radius = 1;
    city->capital = civs[owner].capital_city == id;
    city->port = port;
    city->port_x = x;
    city->port_y = y;
    city->port_region = id;
    population_init_city(id, pop);
}

static void reset_fixture(int active_front, int reachable_unowned, int stability_block) {
    int enemy_x = active_front ? 3 : 8;
    pending_map_size = MAP_SIZE_SMALL;
    initial_civ_count = 2;
    region_size_slider = 34;
    set_active_map_size(pending_map_size);
    diplomacy_reset();
    war_reset();
    war_desire_reset_all();
    stability_decision_reset();
    simulation_reset_state();
    game_clear_world_tiles();
    memset(natural_regions, 0, sizeof(natural_regions));
    memset(cities, 0, sizeof(cities));
    civ_count = 2;
    city_count = 2;
    region_count = 6;
    world_generated = 1;
    init_civ(0, "Crisisland", 0);
    init_civ(1, "Frontierhold", 1);
    init_region(0, 0, 2, 2, 0);
    init_region(1, 1, enemy_x, 2, 1);
    init_region(2, -1, 16, 2, -1);
    init_region(3, -1, 18, 2, -1);
    init_region(4, -1, 20, 2, -1);
    init_region(5, -1, 22, 2, -1);
    if (active_front) add_region_neighbor(0, 1);
    if (reachable_unowned) add_region_neighbor(0, 2);
    add_region_neighbor(2, 3);
    add_region_neighbor(3, 4);
    add_region_neighbor(4, 5);
    init_city(0, 0, "Crisis City", 2, 2, 5000, 1);
    init_city(1, 1, "Border City", enemy_x, 2, 2200, 0);
    civs[0].resource_pressure = 95;
    civs[0].treasury = 0;
    civs[0].treasury_last_annual_balance = -650;
    civs[0].treasury_last_deficit = 650;
    civs[0].treasury_deficit_years = 5;
    if (stability_block) civs[0].disorder = 80;
    terrain_stats_invalidate_cache();
    world_recalculate_territory();
    population_sync_all();
    maritime_rebuild_routes();
    diplomacy_borders_mark_dirty();
    diplomacy_mark_contacts_dirty();
    diplomacy_update_contacts();
    if (stability_block) stability_decision_update_month(0);
}

static DiplomacyRelation probe_relation(int border_tension) {
    DiplomacyRelation relation;
    memset(&relation, 0, sizeof(relation));
    relation.state = DIPLOMACY_TENSE;
    relation.relation_score = 35;
    relation.border_tension = border_tension;
    relation.resource_conflict = 80;
    relation.contact_kind = DIP_CONTACT_LAND_BORDER;
    return relation;
}

static int write_war_case(FILE *summary, const char *name, int active_front,
                          int reachable_unowned, int stability_block) {
    WarDesireBreakdown out;
    int ok;
    reset_fixture(active_front, reachable_unowned, stability_block);
    out = war_desire_calculate(0, 1, probe_relation(74));
    ok = 1;
    if (active_front && !reachable_unowned && !stability_block) ok = out.final_desire > 0;
    if (active_front && reachable_unowned) ok = out.frontier_penalty > 0;
    if (!active_front) ok = out.final_desire == 0 && out.result == WAR_DESIRE_RESULT_NO_FRONT;
    if (stability_block) ok = out.final_desire == 0 && out.result == WAR_DESIRE_RESULT_STABILITY;
    fprintf(summary,
            "case=%s ok=%d final=%d pre=%d raw=%d crisis=%d open_targets=%d global_unowned=%d frontier_penalty=%d result=%d reason=\"%s\"\n",
            name, ok, out.final_desire, out.pre_stability_desire, out.raw_desire,
            out.crisis_score, out.open_target_count, out.global_unowned_percent,
            out.frontier_penalty, out.result, out.reason);
    return ok;
}

static int write_population_case(FILE *summary) {
    PopulationSummary summary_pop;
    TerrainStats stats;
    PopulationDiagnostics low;
    PopulationDiagnostics high;
    int expected_birth_percent;
    int expected_deaths;
    int ok;
    reset_fixture(1, 0, 0);
    memset(&summary_pop, 0, sizeof(summary_pop));
    memset(&stats, 0, sizeof(stats));
    summary_pop.total = 6000;
    summary_pop.male = 3000;
    summary_pop.female = 3000;
    summary_pop.children = 1300;
    summary_pop.working = 4200;
    summary_pop.elder = 500;
    summary_pop.fertile = 2100;
    summary_pop.recruitable = 1600;
    summary_pop.carrying_capacity = 10000;
    summary_pop.pressure = 60;
    summary_pop.cohorts[POP_AGE_18_24].male = 520;
    summary_pop.cohorts[POP_AGE_25_39].male = 720;
    summary_pop.cohorts[POP_AGE_40_54].male = 420;
    stats.food = 5;
    stats.water = 5;
    stats.habitability = 5;
    civs[0].resource_pressure = 5;
    low = population_diagnostics_for_summary(0, summary_pop, stats);
    civs[0].resource_pressure = 95;
    high = population_diagnostics_for_summary(0, summary_pop, stats);
    expected_birth_percent = population_birth_multiplier_percent(high.effective_pressure);
    expected_deaths = population_pressure_deaths_estimate(summary_pop, high.effective_pressure) +
                      population_natural_age_deaths_estimate(summary_pop) +
                      population_child_stress_deaths_estimate(summary_pop, stats);
    ok = high.birth_multiplier_percent == expected_birth_percent &&
         high.estimated_total_deaths == expected_deaths &&
         high.birth_multiplier_percent < low.birth_multiplier_percent;
    fprintf(summary,
            "case=population_pressure ok=%d low_effective=%d low_birth_mult=%d high_effective=%d high_birth_mult=%d births_per_month=%d pressure_deaths_per_month=%d natural_or_age_deaths_per_month=%d child_stress_deaths_per_month=%d total_deaths_per_month=%d net_per_month=%d\n",
            ok, low.effective_pressure, low.birth_multiplier_percent, high.effective_pressure,
            high.birth_multiplier_percent, high.estimated_monthly_births,
            high.estimated_pressure_deaths, high.estimated_natural_age_deaths,
            high.estimated_child_stress_deaths, high.estimated_total_deaths,
            high.estimated_net_monthly_change);
    return ok;
}

static void fill_probe_summary(PopulationSummary *pop, int total, int capacity) {
    static const int dist[POP_COHORT_COUNT] = {10, 22, 10, 22, 18, 9, 6, 3};
    int i;
    int remaining = total;
    memset(pop, 0, sizeof(*pop));
    for (i = 0; i < POP_COHORT_COUNT; i++) {
        int amount = i == POP_COHORT_COUNT - 1 ? remaining : total * dist[i] / 100;
        pop->cohorts[i].male = amount / 2;
        pop->cohorts[i].female = amount - pop->cohorts[i].male;
        remaining -= amount;
    }
    pop->male = total / 2;
    pop->female = total - pop->male;
    pop->total = total;
    pop->children = total * 32 / 100;
    pop->working = total * 59 / 100;
    pop->elder = total - pop->children - pop->working;
    pop->fertile = total * 25 / 100;
    pop->recruitable = total * 20 / 100;
    pop->carrying_capacity = capacity;
    pop->pressure = capacity > 0 ? total * 100 / capacity : 0;
}

static void write_diag_line(FILE *summary, const char *name, int total, int capacity,
                            int national_pressure, int treasury, int cap, int balance,
                            int deficit_years) {
    PopulationSummary pop;
    TerrainStats stats;
    PopulationDiagnostics diag;
    memset(&stats, 0, sizeof(stats));
    fill_probe_summary(&pop, total, capacity);
    stats.food = 5;
    stats.water = 5;
    stats.habitability = 5;
    civs[0].resource_pressure = national_pressure;
    civs[0].treasury = treasury;
    civs[0].treasury_cap = cap;
    civs[0].treasury_last_annual_balance = balance;
    civs[0].treasury_deficit_years = deficit_years;
    diag = population_diagnostics_for_summary(0, pop, stats);
    fprintf(summary,
            "population_diag case=%s population=%d capacity=%d usage=%d actual_pressure=%d national_resource_pressure=%d capacity_overload_pressure=%d birth_multiplier=%d births_per_month=%d pressure_deaths_per_month=%d natural_or_age_deaths_per_month=%d child_stress_deaths_per_month=%d total_deaths_per_month=%d net_per_month=%d treasury=%d cap=%d surplus_deficit=%d deficit_years=%d\n",
            name, pop.total, pop.carrying_capacity, pop.pressure, diag.effective_pressure,
            diag.national_resource_pressure, diag.local_overcapacity_pressure,
            diag.birth_multiplier_percent, diag.estimated_monthly_births,
            diag.estimated_pressure_deaths, diag.estimated_natural_age_deaths,
            diag.estimated_child_stress_deaths, diag.estimated_total_deaths,
            diag.estimated_net_monthly_change, treasury, cap, balance, deficit_years);
}

static void tune_region_capacity(int id, int tiles) {
    if (id < 0 || id >= region_count) return;
    natural_regions[id].tile_count = tiles;
    natural_regions[id].average_stats.food = 5;
    natural_regions[id].average_stats.water = 5;
    natural_regions[id].average_stats.pop_capacity = 5;
    natural_regions[id].average_stats.habitability = 5;
    natural_regions[id].total_stats = natural_regions[id].average_stats;
}

static void paint_city_province(int city_id, int owner, int cx, int cy, int radius) {
    int dx, dy;
    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            int x = cx + dx, y = cy + dy;
            if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H || dx * dx + dy * dy > radius * radius) continue;
            world[y][x].geography = GEO_PLAIN;
            world[y][x].climate = CLIMATE_CONTINENTAL;
            world[y][x].ecology = ECO_GRASSLAND;
            world[y][x].resource = RESOURCE_FEATURE_NONE;
            world[y][x].owner = owner;
            world[y][x].province_id = city_id;
            world[y][x].region_id = city_id;
        }
    }
}

static void setup_population_normal_fixture(void) {
    reset_fixture(1, 0, 0);
    init_region(0, 0, 24, 20, 0);
    init_city(0, 0, "Crisis City", 24, 20, 5200, 1);
    tune_region_capacity(0, 80);
    paint_city_province(0, 0, 24, 20, 9);
    civs[0].resource_pressure = 8;
    civs[0].treasury = 740;
    civs[0].treasury_last_annual_balance = 90;
    civs[0].treasury_last_deficit = 0;
    civs[0].treasury_deficit_years = 0;
    world_invalidate_region_cache();
    terrain_stats_invalidate_cache();
    population_sync_all();
}

static void setup_population_top6_fixture(void) {
    static const int ids[] = {0, 2, 3, 4, 5, 6, 7, 8};
    static const int pops[] = {11000, 8000, 15000, 13000, 7000, 6000, 5000, 1000};
    static const char *names[] = {"Capital", "North Port", "Ironford", "Canal", "Hilltown", "Market", "Plainfield", "Outpost"};
    static const int xs[] = {12, 28, 44, 60, 76, 92, 108, 124};
    int i;
    reset_fixture(1, 0, 0);
    region_count = 9;
    city_count = 9;
    init_region(0, 0, xs[0], 18, 0);
    for (i = 2; i < 9; i++) init_region(i, 0, xs[i - 1], 18, i);
    for (i = 0; i < 9; i++) tune_region_capacity(i, 40);
    for (i = 0; i < 8; i++) {
        init_city(ids[i], 0, names[i], xs[i], 18, pops[i], ids[i] == 0 || ids[i] == 2);
        paint_city_province(ids[i], 0, xs[i], 18, 4);
    }
    civs[0].resource_pressure = 92;
    civs[0].treasury = 0;
    civs[0].treasury_last_annual_balance = -650;
    civs[0].treasury_last_deficit = 650;
    civs[0].treasury_deficit_years = 5;
    world_invalidate_region_cache();
    terrain_stats_invalidate_cache();
    population_sync_all();
}

static int write_population_diagnostics_cases(FILE *summary) {
    int ids[POPULATION_TOP_CITY_COUNT];
    int count = 0;
    int ok;
    reset_fixture(1, 0, 0);
    write_diag_line(summary, "normal_low_pressure", 5200, 12000, 8, 740, 1200, 90, 0);
    write_diag_line(summary, "near_capacity", 9500, 10000, 12, 650, 1200, 40, 0);
    write_diag_line(summary, "overloaded_capacity", 14000, 10000, 20, 610, 1200, -20, 1);
    write_diag_line(summary, "buffered_resource_deficit", 9000, 12000, 65, 700, 1200, -260, 1);
    write_diag_line(summary, "treasury_zero_deficit", 9000, 12000, 95, 0, 1200, -650, 5);
    setup_population_top6_fixture();
    population_country_city_count_cached(0, &count);
    population_country_top_city_ids_cached(0, ids, POPULATION_TOP_CITY_COUNT);
    ok = count == 8 && ids[0] == 3 && ids[1] == 4 && ids[2] == 0 &&
         ids[3] == 2 && ids[4] == 5 && ids[5] == 6;
    fprintf(summary, "top6_case ok=%d city_count=%d top_ids=%d/%d/%d/%d/%d/%d remaining=%d\n",
            ok, count, ids[0], ids[1], ids[2], ids[3], ids[4], ids[5],
            count > POPULATION_TOP_CITY_COUNT ? count - POPULATION_TOP_CITY_COUNT : 0);
    return ok;
}

static int write_bmp_from_bits(const char *path, BITMAPINFO *info, void *bits, int width, int height) {
    BITMAPFILEHEADER file_header;
    FILE *file;
    DWORD image_size = (DWORD)(width * height * 4);
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

static int render_population_tab_bmp(const char *path, int language) {
    const int width = 560;
    const int height = 900;
    BITMAPINFO info;
    void *bits = NULL;
    HDC screen = GetDC(NULL);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bitmap;
    HGDIOBJ old_bitmap;
    HFONT font;
    RECT client = {0, 0, width, height};
    HBRUSH brush;
    UiCursor cursor;
    const RenderSnapshot *snapshot;
    int ok = 0;
    DWORD start_ms = GetTickCount();
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bitmap || !bits || !mem) goto cleanup;
    old_bitmap = SelectObject(mem, bitmap);
    brush = CreateSolidBrush(RGB(244, 242, 236));
    FillRect(mem, &client, brush);
    DeleteObject(brush);
    font = CreateFontW(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                       DEFAULT_PITCH | FF_SWISS,
                       language ? L"Microsoft YaHei UI" : L"Segoe UI");
    SelectObject(mem, font);
    render_snapshot_init();
    render_snapshot_cache_update_all();
    render_snapshot_publish_from_live_state();
    snapshot = render_snapshot_acquire();
    if (snapshot) {
        ui_language = language;
        render_context_begin(snapshot);
        cursor = ui_cursor(18, 18, width - 36, height - 18);
        draw_country_population_tab(mem, client, &cursor, 0, font);
        render_context_end();
        render_snapshot_release(snapshot);
        ok = write_bmp_from_bits(path, &info, bits, width, height);
    }
    last_population_render_ms = GetTickCount() - start_ms;
    render_snapshot_shutdown();
    SelectObject(mem, old_bitmap);
    DeleteObject(font);
cleanup:
    if (bitmap) DeleteObject(bitmap);
    if (mem) DeleteDC(mem);
    if (screen) ReleaseDC(NULL, screen);
    return ok;
}

static int write_population_render_case(FILE *summary) {
    int ok_en, ok_zh, ok_top_en, ok_top_zh;
    DWORD normal_en_ms, normal_zh_ms, top_en_ms, top_zh_ms;
    setup_population_normal_fixture();
    ok_en = render_population_tab_bmp(CRISIS_PROBE_DIR "/population_tab_normal_en.bmp", 0);
    normal_en_ms = last_population_render_ms;
    ok_zh = render_population_tab_bmp(CRISIS_PROBE_DIR "/population_tab_normal_zh.bmp", 1);
    normal_zh_ms = last_population_render_ms;
    setup_population_top6_fixture();
    ok_top_en = render_population_tab_bmp(CRISIS_PROBE_DIR "/population_tab_top6_deficit_en.bmp", 0);
    top_en_ms = last_population_render_ms;
    ok_top_zh = render_population_tab_bmp(CRISIS_PROBE_DIR "/population_tab_top6_deficit_zh.bmp", 1);
    top_zh_ms = last_population_render_ms;
    fprintf(summary,
            "case=population_tab_render ok=%d normal_en=%s normal_zh=%s top6_deficit_en=%s top6_deficit_zh=%s render_ms=%lu/%lu/%lu/%lu expected_labels=\"Population Capacity;Population Structure;Actual Population Pressure;Monthly Net Change;Treasury Buffer;City Population Top 6\"\n",
            ok_en && ok_zh && ok_top_en && ok_top_zh,
            CRISIS_PROBE_DIR "/population_tab_normal_en.bmp",
            CRISIS_PROBE_DIR "/population_tab_normal_zh.bmp",
            CRISIS_PROBE_DIR "/population_tab_top6_deficit_en.bmp",
            CRISIS_PROBE_DIR "/population_tab_top6_deficit_zh.bmp",
            (unsigned long)normal_en_ms, (unsigned long)normal_zh_ms,
            (unsigned long)top_en_ms, (unsigned long)top_zh_ms);
    return ok_en && ok_zh && ok_top_en && ok_top_zh;
}

int run_crisis_probe(void) {
    FILE *summary;
    int ok = 1;
    ensure_probe_dirs();
    summary = fopen(CRISIS_PROBE_DIR "/summary.txt", "w");
    if (!summary) return 1;
    ok &= write_war_case(summary, "crisis_no_reachable_land_active_front", 1, 0, 0);
    ok &= write_war_case(summary, "crisis_reachable_unowned_land", 1, 1, 0);
    ok &= write_war_case(summary, "crisis_no_active_front", 0, 0, 0);
    ok &= write_war_case(summary, "crisis_stability_reorganization", 1, 0, 1);
    ok &= write_population_case(summary);
    ok &= write_population_diagnostics_cases(summary);
    ok &= write_population_render_case(summary);
    fprintf(summary, "overall=%s\n", ok ? "PASS" : "FAIL");
    fclose(summary);
    return ok ? 0 : 1;
}
