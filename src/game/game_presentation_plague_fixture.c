#include "game/game_presentation_plague_fixture.h"

#include "render/panel_plague_page.h"
#include "render/render_common.h"
#include "render/render_context.h"
#include "render/render_panel_internal.h"
#include "sim/plague_rules.h"
#include "ui/ui_pressed_state.h"
#include "ui/ui_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_bmp(const char *path, const BITMAPINFO *info,
                     const void *bits, int width, int height) {
    BITMAPFILEHEADER header;
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    memset(&header, 0, sizeof(header));
    header.bfType = 0x4D42;
    header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + (DWORD)(width * height * 4);
    fwrite(&header, sizeof(header), 1, file);
    fwrite(&info->bmiHeader, sizeof(BITMAPINFOHEADER), 1, file);
    fwrite(bits, (size_t)(width * height * 4), 1, file);
    fclose(file);
    return 1;
}

void plague_probe_fill_history(PlagueEpisodeHistory *history, int episode_id,
                               int newest_offset) {
    if (!history) return;
    memset(history, 0, sizeof(*history));
    history->episode_id = episode_id;
    history->size = (PlagueSize)(PLAGUE_SIZE_SMALL + newest_offset % 3);
    history->severity = history->size == PLAGUE_SIZE_SMALL ? 3 :
                        history->size == PLAGUE_SIZE_MEDIUM ? 7 : 10;
    history->name_id = newest_offset;
    history->name_cycle = 1;
    history->start_month = (100 - newest_offset * 13) * 12;
    history->duration_months = 48 + newest_offset * 43;
    history->end_month = history->start_month + history->duration_months;
    history->spores_initial = 20 + newest_offset * 8;
    history->spores_used = newest_offset == 6 ? 0 :
                            newest_offset == 0 ? history->spores_initial :
                                                 12 + newest_offset * 5;
    history->infected_city_count = 8 + newest_offset * 4;
    history->affected_country_count = 2 + newest_offset;
    history->total_deaths = 12000 + (int64_t)newest_offset * 7350;
}

static void fill_history_view(RenderSnapshot *snapshot, int count) {
    static const char *names_en[PLAGUE_VIEW_HISTORY_COUNT] = {
        "Lastbell Plague", "Bluebone Pestilence", "Coldstar Malady",
        "Redmist Sickness", "Blackvein Fever", "Glassrain Pox",
        "Pale Orchard Blight"
    };
    static const char *names_zh[PLAGUE_VIEW_HISTORY_COUNT] = {
        "终钟疫", "蓝骨瘟", "寒星病", "红雾症", "黑脉热", "琉雨痘", "白园枯疫"
    };
    int i;
    count = clamp(count, 0, PLAGUE_VIEW_HISTORY_COUNT);
    snapshot->plague_state.next_scheduled_check_month = 140 * 12;
    snapshot->plague_state.starts_in_rolling_window = 2;
    snapshot->plague_state.recent_history_count = count;
    for (i = 0; i < count; i++) {
        plague_probe_fill_history(&snapshot->plague_state.recent_history[i],
                                  107 - i, i);
        snprintf(snapshot->plague_names.history_en[i],
                 sizeof(snapshot->plague_names.history_en[i]), "%s", names_en[i]);
        snprintf(snapshot->plague_names.history_zh[i],
                 sizeof(snapshot->plague_names.history_zh[i]), "%s", names_zh[i]);
    }
}

static void fill_base(RenderSnapshot *snapshot, int history_count) {
    int i;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->world_generated = 1;
    snapshot->year = 121;
    snapshot->month = 7;
    snapshot->map_w = 64;
    snapshot->map_h = 36;
    snapshot->civ_count = 2;
    snapshot->city_count = 3;
    snapshot->revision = 1700 + history_count;
    snapshot->plague_revision = 171 + history_count;
    snapshot->civs_revision = 31;
    snapshot->cities_revision = 41;
    for (i = 0; i < snapshot->civ_count; i++) {
        SnapshotCiv *civ = &snapshot->civs[i];
        civ->alive = 1;
        civ->id = i;
        civ->uid = 8000 + i;
        civ->symbol = (char)('A' + i);
        civ->color = RGB(78 + i * 50, 128, 98 + i * 35);
        snprintf(civ->name_en, sizeof(civ->name_en), "River Country %d", i + 1);
        snprintf(civ->name_zh, sizeof(civ->name_zh), "河国%d", i + 1);
    }
    for (i = 0; i < snapshot->city_count; i++) {
        SnapshotCity *city = &snapshot->cities[i];
        city->alive = 1;
        city->owner = i % 2;
        city->population = 25000 + i * 7000;
        snprintf(city->name, sizeof(city->name), "River City %d", i + 1);
    }
    snapshot->plague_state.immunity_city_count[0] = 12;
    snapshot->plague_state.immunity_city_count[1] = 8;
    snapshot->plague_state.immunity_city_count[2] = 5;
    snapshot->plague_state.immunity_city_count[3] = 2;
    fill_history_view(snapshot, history_count);
}

void plague_probe_fill_active(RenderSnapshot *snapshot, PlagueSize size,
                              int history_count) {
    PlagueEpisodeState *episode;
    PlagueSizeRules rules;
    int severity = size == PLAGUE_SIZE_SMALL ? 3 :
                   size == PLAGUE_SIZE_MEDIUM ? 7 : 10;
    int absolute_month;
    int age;
    int i;
    if (!snapshot) return;
    fill_base(snapshot, history_count);
    snapshot->plague_revision += 10 * (int)size;
    episode = &snapshot->plague_state.episode;
    episode->active = 1;
    episode->episode_id = 500 + size;
    episode->size = size;
    episode->severity = severity;
    episode->name_id = size - 1;
    episode->name_cycle = 1;
    episode->start_month = snapshot->year * 12 + snapshot->month - 39;
    episode->origin_city_id = 0;
    episode->origin_civ_id = 0;
    snprintf(episode->origin_city_name, sizeof(episode->origin_city_name),
             "Blue River City");
    snprintf(episode->origin_civ_name_en, sizeof(episode->origin_civ_name_en),
             "Blue River Realm");
    snprintf(episode->origin_civ_name_zh, sizeof(episode->origin_civ_name_zh),
             "青河国");
    episode->frozen_occupied_cities = 80;
    episode->spores_initial = size == PLAGUE_SIZE_SMALL ? 20 :
                              size == PLAGUE_SIZE_MEDIUM ? 44 : 56;
    episode->spores_remaining = episode->spores_initial / 2;
    episode->active_city_count = 3;
    episode->ever_infected_city_count = 14;
    episode->current_generation = size == PLAGUE_SIZE_SMALL ? 2 : 3;
    episode->maximum_generation_reached = episode->current_generation;
    episode->current_affected_country_count = 2;
    episode->ever_affected_country_count = 4;
    episode->current_month_deaths = 321;
    episode->total_deaths = 18432;
    snapshot->plague_state.rolling_12_month_deaths = 4392;
    snapshot->plague_state.disorder_current = 34;
    snapshot->plague_state.disorder_target = 46;
    absolute_month = snapshot->year * 12 + snapshot->month - 1;
    age = max(0, absolute_month - episode->start_month);
    snapshot->plague_state.projected_immunity_percent =
        plague_rules_immunity_percent_for_duration(age);
    plague_rules_next_immunity_tier(
        age, &snapshot->plague_state.next_immunity_percent,
        &snapshot->plague_state.months_to_next_immunity);
    if (plague_rules_size_values(size, &rules)) {
        snapshot->plague_state.maximum_generation_index = rules.maximum_generation;
        snapshot->plague_state.reachable_generation_layers =
            rules.maximum_generation + 1;
    }
    snprintf(snapshot->plague_names.active_en,
             sizeof(snapshot->plague_names.active_en), "Azure River Fever");
    snprintf(snapshot->plague_names.active_zh,
             sizeof(snapshot->plague_names.active_zh), "青河热");
    snapshot->plague_active = 1;
    snapshot->plague_state.active_city_count = snapshot->city_count;
    snapshot->plague_state.current_country_count = snapshot->civ_count;
    for (i = 0; i < snapshot->city_count; i++) {
        snapshot->plague_state.active_city_ids[i] = i;
        snapshot->cities[i].plague_active = 1;
        snapshot->cities[i].plague_severity = severity;
        snapshot->cities[i].plague_months_left = 18 + i * 6;
        snapshot->cities[i].plague_deaths_total = 2100 + i * 900;
        snapshot->civs[i % 2].plague_active_count++;
        snapshot->civs[i % 2].plague_peak_severity = severity;
        snapshot->civs[i % 2].plague_deaths_total +=
            snapshot->cities[i].plague_deaths_total;
    }
    for (i = 0; i < snapshot->civ_count; i++) {
        snapshot->plague_state.current_country_ids[i] = i;
    }
}

void plague_probe_fill_inactive(RenderSnapshot *snapshot, int history_count) {
    if (!snapshot) return;
    fill_base(snapshot, history_count);
    snapshot->plague_revision += 80;
}

void plague_probe_fill_impact(RenderSnapshot *snapshot, int country_count,
                             int city_count) {
    SnapshotPlagueImpact *impact;
    int i;
    if (!snapshot) return;
    impact = &snapshot->plague_impact;
    memset(impact, 0, sizeof(*impact));
    impact->active = snapshot->plague_state.episode.active;
    if (!impact->active) return;
    impact->episode_id = snapshot->plague_state.episode.episode_id;
    impact->country_count = clamp(country_count, 0, MAX_CIVS);
    impact->city_count = clamp(city_count, 0,
                               RENDER_SNAPSHOT_PLAGUE_TOP_CITY_COUNT);
    impact->candidate_city_count = 19;
    impact->total_episode_deaths = 92000;
    for (i = 0; i < impact->country_count; i++) {
        SnapshotPlagueCountryImpact *country = &impact->countries[i];
        country->civ_id = i;
        country->civ_uid = 9100 + i;
        country->alive = i != 2 && i != 11;
        country->symbol = (char)('A' + i % 26);
        country->color = i == 0 ? RGB(28, 52, 78) :
                         i == 1 ? RGB(226, 210, 118) :
                         RGB(75 + i * 9 % 150, 95 + i * 13 % 120,
                             110 + i * 7 % 120);
        snprintf(country->name_en, sizeof(country->name_en),
                 "Current Realm %02d", i + 1);
        snprintf(country->name_zh, sizeof(country->name_zh),
                 "现属国%02d", i + 1);
        country->current_population = 820000 - (int64_t)i * 27000;
        country->current_infected_city_count = i % 3;
        country->ever_infected_city_count = 2 + i;
        country->episode_deaths = 18000 - (int64_t)i * 900;
        country->death_share_basis_points = 1956 - i * 71;
        country->peak_severity = 9;
        country->status = i == 2 ? SNAPSHOT_PLAGUE_IMPACT_NO_LONGER_EXISTS :
                          country->current_infected_city_count > 0 ?
                          SNAPSHOT_PLAGUE_IMPACT_ACTIVE :
                          SNAPSHOT_PLAGUE_IMPACT_RECOVERED;
    }
    for (i = 0; i < impact->city_count; i++) {
        SnapshotPlagueCityImpact *city = &impact->cities[i];
        city->city_id = 40 + i;
        snprintf(city->city_name, sizeof(city->city_name),
                 "Impact City %d", i + 1);
        city->current_owner_id = 9 + i;
        city->current_owner_uid = 9900 + i;
        city->current_owner_alive = i != 2;
        city->current_owner_symbol = (char)('J' + i);
        snprintf(city->current_owner_name_en,
                 sizeof(city->current_owner_name_en), "Current Owner %d", i + 1);
        snprintf(city->current_owner_name_zh,
                 sizeof(city->current_owner_name_zh), "现领国%d", i + 1);
        city->current_population = i == 2 ? 0 : 95000 - i * 11000;
        city->episode_deaths = 9500 - (int64_t)i * 1250;
        city->months_remaining = 18 - i * 2;
        city->generation = i;
        city->status = i == 1 ? SNAPSHOT_PLAGUE_IMPACT_RECOVERED :
                       i == 2 ? SNAPSHOT_PLAGUE_IMPACT_NO_LONGER_EXISTS :
                                SNAPSHOT_PLAGUE_IMPACT_ACTIVE;
    }
}

static int prepare_surface(int width, int height, HDC *out_screen,
                           HDC *out_hdc, HBITMAP *out_bitmap,
                           HBITMAP *out_old, BITMAPINFO *out_info,
                           void **out_bits) {
    HDC screen = GetDC(NULL);
    HDC hdc = screen ? CreateCompatibleDC(screen) : NULL;
    HBITMAP bitmap;
    if (!screen || !hdc) {
        if (hdc) DeleteDC(hdc);
        if (screen) ReleaseDC(NULL, screen);
        return 0;
    }
    memset(out_info, 0, sizeof(*out_info));
    out_info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    out_info->bmiHeader.biWidth = width;
    out_info->bmiHeader.biHeight = -height;
    out_info->bmiHeader.biPlanes = 1;
    out_info->bmiHeader.biBitCount = 32;
    out_info->bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, out_info, DIB_RGB_COLORS,
                              out_bits, NULL, 0);
    if (!bitmap || !*out_bits) {
        if (bitmap) DeleteObject(bitmap);
        DeleteDC(hdc);
        ReleaseDC(NULL, screen);
        return 0;
    }
    *out_screen = screen;
    *out_hdc = hdc;
    *out_bitmap = bitmap;
    *out_old = SelectObject(hdc, bitmap);
    return 1;
}

static void release_surface(HDC screen, HDC hdc, HBITMAP bitmap,
                            HBITMAP old_bitmap) {
    SelectObject(hdc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ReleaseDC(NULL, screen);
}

static int render_artifact(RenderSnapshot *snapshot,
                           const char *file_name, int language,
                           int panel_width, PanelTab outer_panel,
                           PlaguePanelTab tab,
                           PlagueHistoryMetric metric, int impact_page,
                           int scroll_delta, int hover_target,
                           UiPressedControlKind pressed_kind,
                           int pressed_index,
                           PlagueProbeUiPrepareFn prepare,
                           void *prepare_context) {
    const int width = panel_width + 220;
    const int height = 1040;
    HDC screen;
    HDC hdc;
    BITMAPINFO info;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    void *bits = NULL;
    RECT client = {0, 0, width, height};
    char path[256];
    int old_language = ui_language;
    int old_panel = panel_tab;
    int old_width = side_panel_w;
    int old_collapsed = side_panel_collapsed;
    int ok;
    if (!snapshot || !file_name || !prepare_surface(width, height, &screen,
            &hdc, &bitmap, &old_bitmap, &info, &bits)) return 0;
    fill_rect(hdc, client, RGB(20, 26, 29));
    ui_language = language;
    side_panel_w = panel_width;
    side_panel_collapsed = 0;
    panel_tab = outer_panel;
    plague_panel_reset_scroll();
    ui_plague_panel_set_main_tab(tab);
    ui_plague_panel_set_history_metric(metric);
    ui_plague_panel_set_hover_target(hover_target);
    ui_pressed_control_set(NULL, pressed_kind, pressed_index);
    if (prepare) prepare(snapshot, prepare_context);
    render_context_begin(snapshot);
    draw_side_panel(hdc, client);
    if (impact_page > 0) ui_plague_panel_set_impact_page(impact_page);
    if (scroll_delta != 0) plague_panel_scroll(client, scroll_delta);
    if (impact_page > 0 || scroll_delta != 0) {
        fill_rect(hdc, client, RGB(20, 26, 29));
        draw_side_panel(hdc, client);
    }
    render_context_end();
    snprintf(path, sizeof(path), "%s/%s", PLAGUE_PRESENTATION_PROBE_DIR,
             file_name);
    ok = write_bmp(path, &info, bits, width, height);
    ui_pressed_control_clear(NULL);
    ui_plague_panel_clear_hover();
    ui_language = old_language;
    panel_tab = old_panel;
    side_panel_w = old_width;
    side_panel_collapsed = old_collapsed;
    release_surface(screen, hdc, bitmap, old_bitmap);
    return ok;
}

int plague_probe_render_artifact(RenderSnapshot *snapshot,
                                 const char *file_name, int language,
                                 int panel_width, PlaguePanelTab tab,
                                 PlagueHistoryMetric metric, int impact_page,
                                 int scroll_delta) {
    return render_artifact(snapshot, file_name, language, panel_width,
                           PANEL_PLAGUE, tab, metric, impact_page, scroll_delta,
                           UI_PLAGUE_PANEL_HIT_NONE, UI_PRESSED_NONE, -1,
                           NULL, NULL);
}

int plague_probe_render_interaction_artifact(
    RenderSnapshot *snapshot, const char *file_name, int language,
    int panel_width, PlaguePanelTab tab, PlagueHistoryMetric metric,
    int hover_target, UiPressedControlKind pressed_kind, int pressed_index) {
    return render_artifact(snapshot, file_name, language, panel_width,
                           PANEL_PLAGUE, tab, metric, 0, 0, hover_target,
                           pressed_kind, pressed_index, NULL, NULL);
}

int plague_probe_render_prepared_artifact(
    RenderSnapshot *snapshot, const char *file_name, int language,
    int panel_width, PlaguePanelTab tab, PlagueHistoryMetric metric,
    int hover_target, UiPressedControlKind pressed_kind, int pressed_index,
    PlagueProbeUiPrepareFn prepare, void *context) {
    return render_artifact(snapshot, file_name, language, panel_width,
                           PANEL_PLAGUE, tab, metric, 0, 0, hover_target,
                           pressed_kind, pressed_index, prepare, context);
}

int plague_probe_render_outer_panel_artifact(
    RenderSnapshot *snapshot, const char *file_name, int language,
    int panel_width, PanelTab outer_panel, PlagueProbeUiPrepareFn prepare,
    void *context) {
    return render_artifact(snapshot, file_name, language, panel_width,
                           outer_panel, PLAGUE_PANEL_TAB_LIVE,
                           PLAGUE_HISTORY_METRIC_TYPE, 0, 0,
                           UI_PLAGUE_PANEL_HIT_NONE, UI_PRESSED_NONE, -1,
                           prepare, context);
}

int plague_probe_chart_smoke(const PlagueEpisodeHistory *newest_first,
                             int count, PlaguePanelChartMetric metric) {
    HDC screen;
    HDC hdc;
    BITMAPINFO info;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    void *bits = NULL;
    RECT chart = {0, 0, 500, 300};
    RECT plot = {34, 18, 490, 266};
    RECT hits[PLAGUE_VIEW_HISTORY_COUNT];
    int i;
    if (!prepare_surface(500, 300, &screen, &hdc, &bitmap, &old_bitmap,
                         &info, &bits)) return 0;
    fill_rect(hdc, chart, RGB(26, 34, 37));
    for (i = 0; i < PLAGUE_VIEW_HISTORY_COUNT; i++) {
        hits[i] = (RECT){plot.left + (plot.right - plot.left) * i /
                         PLAGUE_VIEW_HISTORY_COUNT, plot.top,
                         plot.left + (plot.right - plot.left) * (i + 1) /
                         PLAGUE_VIEW_HISTORY_COUNT, chart.bottom};
    }
    plague_panel_chart_draw(hdc, chart, plot, hits, newest_first, count,
                            metric, 0, -1);
    release_surface(screen, hdc, bitmap, old_bitmap);
    return 1;
}
