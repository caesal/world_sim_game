#include "render/panel_plague_impact.h"

#include "render/country_identity_block.h"
#include "render/panel_plague_common.h"
#include "render/render_common.h"
#include "ui/ui_clay_theme.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_pressed_state.h"
#include "ui/ui_types.h"

#include <stdio.h>

static const char *impact_status_label(SnapshotPlagueImpactStatus status) {
    if (status == SNAPSHOT_PLAGUE_IMPACT_ACTIVE) return tr("Active", "感染中");
    if (status == SNAPSHOT_PLAGUE_IMPACT_RECOVERED) return tr("Recovered", "已恢复");
    if (status == SNAPSHOT_PLAGUE_IMPACT_NO_LONGER_EXISTS) {
        return tr("No longer exists", "已不存在");
    }
    return tr("Unavailable", "不可用");
}

static COLORREF status_color(SnapshotPlagueImpactStatus status) {
    UiClaySemanticTone tone = status == SNAPSHOT_PLAGUE_IMPACT_ACTIVE ?
                               UI_CLAY_TONE_WAR :
                               status == SNAPSHOT_PLAGUE_IMPACT_RECOVERED ?
                               UI_CLAY_TONE_PEACE : UI_CLAY_TONE_MUTED;
    return ui_clay_semantic_style(tone).accent;
}

static void draw_section_header(HDC hdc, RECT rect, const char *text) {
    ui_clay_draw_section_header(hdc, rect, text);
}

static void draw_attribution_note(HDC hdc, RECT rect,
                                  const SnapshotPlagueImpact *impact) {
    char deaths[48];
    char text[320];
    plague_panel_format_count64(impact->unattributed_deaths, deaths,
                                sizeof(deaths));
    if (impact->unattributed_city_count > 0 ||
        impact->unattributed_deaths > 0) {
        snprintf(text, sizeof(text),
                 tr("Current-owner attribution; unattributed: %d cities / %s deaths.",
                    "按当前所有者归因；未归因：%d座城市 / %s人死亡。"),
                 impact->unattributed_city_count, deaths);
    } else {
        snprintf(text, sizeof(text), "%s",
                 tr("Deaths and city counts use each city's current owner.",
                    "死亡与城市数按各城市的当前所有者归因。"));
    }
    draw_text_rect(hdc, rect, text, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_LEFT | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);
}

static void format_share(int basis_points, char *out, size_t out_size) {
    int safe = clamp(basis_points, 0, 10000);
    snprintf(out, out_size, "%d.%02d%%", safe / 100, safe % 100);
}

static void draw_pager(HDC hdc, const UiPlagueImpactLayout *layout,
                       int page, int page_count) {
    int hover = ui_plague_panel_hover_target();
    int previous_disabled = page <= 0;
    int next_disabled = page_count <= 0 || page + 1 >= page_count;
    int previous_hover = hover == UI_PLAGUE_PANEL_HIT_IMPACT_PREVIOUS;
    int next_hover = hover == UI_PLAGUE_PANEL_HIT_IMPACT_NEXT;
    char text[48];
    ui_clay_draw_icon_button(hdc, layout->previous, "<",
        ui_clay_state_from_flags(previous_hover,
                                 ui_pressed_control_is_active(
                                     UI_PRESSED_PLAGUE_IMPACT_PAGER, 0),
                                 0, previous_disabled));
    ui_clay_draw_icon_button(hdc, layout->next, ">",
        ui_clay_state_from_flags(next_hover,
                                 ui_pressed_control_is_active(
                                     UI_PRESSED_PLAGUE_IMPACT_PAGER, 1),
                                 0, next_disabled));
    snprintf(text, sizeof(text), tr("Page %d / %d", "第 %d / %d 页"),
             page_count > 0 ? page + 1 : 0, page_count);
    draw_center_text(hdc, layout->page_label, text,
                     ui_theme_color(UI_COLOR_TEXT_MUTED));
}

static void draw_country_row(HDC hdc, RECT row,
                             const SnapshotPlagueCountryImpact *country,
                             int rank, int alternate) {
    RECT marker = row;
    RECT line1 = row;
    RECT rank_rect = row;
    RECT identity_bounds = row;
    RECT status = row;
    RECT line2 = row;
    RECT line3 = row;
    RECT column = row;
    char deaths[48];
    char population[48];
    char share[24];
    char rank_text[24];
    char text[256];
    SIZE status_size;
    int status_width;
    const char *status_text = impact_status_label(country->status);
    const char *country_name = ui_language == UI_LANG_ZH ?
                               country->name_zh : country->name_en;
    fill_rect(hdc, row, alternate ? ui_theme_color(UI_COLOR_PANEL_SOFT) :
              ui_clay_style(UI_CLAY_SURFACE_PANEL, UI_CLAY_STATE_NORMAL).fill);
    marker.right = marker.left + 3;
    fill_rect(hdc, marker, status_color(country->status));
    measure_text_utf8(hdc, status_text, &status_size);
    status_width = clamp(status_size.cx + 8, 58,
                         min(128, (row.right - row.left) * 40 / 100));
    line1 = (RECT){row.left + 9, row.top + 3,
                   row.right - status_width - 11, row.top + 25};
    status = (RECT){max(line1.left, line1.right + 4), line1.top,
                    row.right - 7, line1.bottom};
    line2 = (RECT){line1.left, line1.bottom, row.right - 7, line1.bottom + 20};
    line3 = (RECT){line1.left, line2.bottom, row.right - 7, row.bottom - 3};
    rank_rect = (RECT){line1.left, line1.top, min(line1.left + 34, line1.right),
                       line1.bottom};
    snprintf(rank_text, sizeof(rank_text), "#%d", rank);
    draw_text_rect(hdc, rank_rect, rank_text, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    identity_bounds = (RECT){rank_rect.right + 3, line1.top, line1.right,
                             line1.bottom};
    country_identity_block_draw(
        hdc, identity_bounds, country->symbol ? country->symbol : '-',
        country_name && country_name[0] ? country_name : tr("Unknown", "未知"),
        country->color);
    draw_text_rect(hdc, status, status_text,
                   status_color(country->status),
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_END_ELLIPSIS |
                       DT_NOPREFIX);
    plague_panel_format_count64(country->current_population, population,
                                sizeof(population));
    plague_panel_format_count64(country->episode_deaths, deaths, sizeof(deaths));
    format_share(country->death_share_basis_points, share, sizeof(share));
    column = line2;
    column.right = line2.left + (line2.right - line2.left) * 52 / 100;
    snprintf(text, sizeof(text), "%s %s",
             tr("Current pop.", "当前人口"), population);
    draw_text_rect(hdc, column, text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    column.left = column.right + 5;
    column.right = line2.right;
    snprintf(text, sizeof(text), "%s %s",
             tr("Deaths", "本次死亡"), deaths);
    draw_text_rect(hdc, column, text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_END_ELLIPSIS |
                       DT_NOPREFIX);
    column = line3;
    column.right = line3.left + (line3.right - line3.left) * 38 / 100;
    snprintf(text, sizeof(text), "%s %d/%d", tr("Cities", "城现/总"),
             country->current_infected_city_count,
             country->ever_infected_city_count);
    draw_text_rect(hdc, column, text, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    column.left = column.right + 4;
    column.right = line3.left + (line3.right - line3.left) * 76 / 100;
    snprintf(text, sizeof(text), "%s %s", tr("Share", "占比"), share);
    draw_text_rect(hdc, column, text, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    column.left = column.right + 4;
    column.right = line3.right;
    snprintf(text, sizeof(text), "%s %d/10", tr("Peak", "峰值"),
             country->peak_severity);
    draw_text_rect(hdc, column, text, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_END_ELLIPSIS |
                       DT_NOPREFIX);
}

static void draw_city_row(HDC hdc, RECT row,
                          const SnapshotPlagueCityImpact *city,
                          int rank, int alternate) {
    RECT marker = row;
    RECT line1;
    RECT status;
    RECT line2;
    char deaths[48];
    char population[48];
    char owner[128];
    char text[320];
    const char *owner_name = ui_language == UI_LANG_ZH ?
                             city->current_owner_name_zh :
                             city->current_owner_name_en;
    fill_rect(hdc, row, alternate ? ui_theme_color(UI_COLOR_PANEL_SOFT) :
              ui_clay_style(UI_CLAY_SURFACE_PANEL, UI_CLAY_STATE_NORMAL).fill);
    marker.right = marker.left + 3;
    fill_rect(hdc, marker, status_color(city->status));
    line1 = (RECT){row.left + 9, row.top + 3, row.right - 117,
                   row.top + (row.bottom - row.top) / 2};
    status = (RECT){max(line1.left, line1.right + 4), line1.top,
                    row.right - 7, line1.bottom};
    line2 = (RECT){line1.left, line1.bottom, row.right - 7, row.bottom - 3};
    if (city->current_owner_id >= 0 && owner_name && owner_name[0]) {
        snprintf(owner, sizeof(owner), "%c %s",
                 city->current_owner_symbol ? city->current_owner_symbol : '-',
                 owner_name);
    } else {
        snprintf(owner, sizeof(owner), "%s", tr("No current owner", "无当前所属国"));
    }
    snprintf(text, sizeof(text), "#%d  %s  ·  %s",
             rank, city->city_name[0] ? city->city_name : tr("Unknown city", "未知城市"),
             owner);
    draw_text_rect(hdc, line1, text, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    draw_text_rect(hdc, status, impact_status_label(city->status),
                   status_color(city->status),
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_END_ELLIPSIS |
                       DT_NOPREFIX);
    plague_panel_format_count64(city->current_population, population,
                                sizeof(population));
    plague_panel_format_count64(city->episode_deaths, deaths, sizeof(deaths));
    if (city->status == SNAPSHOT_PLAGUE_IMPACT_ACTIVE) {
        RECT value = line2;
        value.right = line2.left + (line2.right - line2.left) * 30 / 100;
        snprintf(text, sizeof(text), "%s %s", tr("Pop", "人口"), population);
        draw_text_rect(hdc, value, text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        value.left = value.right + 3;
        value.right = line2.left + (line2.right - line2.left) * 59 / 100;
        snprintf(text, sizeof(text), "%s %s", tr("Deaths", "死亡"), deaths);
        draw_text_rect(hdc, value, text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        value.left = value.right + 3;
        value.right = line2.left + (line2.right - line2.left) * 82 / 100;
        snprintf(text, sizeof(text), "%s %dm", tr("Left", "剩余"),
                 city->months_remaining);
        draw_text_rect(hdc, value, text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        value.left = value.right + 3;
        value.right = line2.right;
        snprintf(text, sizeof(text), "%s %d", tr("Gen", "代"), city->generation);
        draw_text_rect(hdc, value, text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                       DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_END_ELLIPSIS |
                           DT_NOPREFIX);
        return;
    } else {
        snprintf(text, sizeof(text), "%s %s  ·  %s %s  ·  %s %d",
                 tr("Pop. now", "当前人口"), population,
                 tr("Deaths", "死亡"), deaths,
                 tr("Gen.", "代"), city->generation);
    }
    draw_text_rect(hdc, line2, text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
}

static void draw_inactive(HDC hdc, const UiPlagueImpactLayout *layout) {
    RECT message = {layout->country_header.left, layout->country_header.top,
                    layout->country_header.right,
                    layout->country_header.top + 110};
    fill_rect(hdc, message, ui_theme_color(UI_COLOR_PANEL_SOFT));
    draw_center_text(hdc, message,
                     tr("No active impact data", "暂无活跃影响数据"),
                     ui_theme_color(UI_COLOR_TEXT_MUTED));
}

void plague_panel_impact_draw(HDC hdc, const UiPlagueImpactLayout *layout,
                              const RenderSnapshot *snapshot) {
    const SnapshotPlagueImpact *impact = &snapshot->plague_impact;
    int page;
    int page_count;
    int start;
    int shown;
    int i;
    ui_plague_panel_set_impact_country_count(
        impact->active ? impact->country_count : 0);
    if (!impact->active) {
        draw_inactive(hdc, layout);
        return;
    }
    page = ui_plague_panel_impact_page();
    page_count = ui_plague_panel_impact_page_count();
    start = page * UI_PLAGUE_IMPACT_PAGE_SIZE;
    shown = clamp(impact->country_count - start, 0, UI_PLAGUE_IMPACT_PAGE_SIZE);
    draw_section_header(hdc, layout->country_header,
                        tr("Country Impact", "国家影响"));
    draw_attribution_note(hdc, layout->country_note, impact);
    draw_pager(hdc, layout, page, page_count);
    for (i = 0; i < shown; i++) {
        draw_country_row(hdc, layout->country_rows[i],
                         &impact->countries[start + i], start + i + 1, i & 1);
    }
    draw_section_header(hdc, layout->city_header,
                        tr("City Top 5 by Plague Deaths", "城市瘟疫死亡前五"));
    for (i = 0; i < impact->city_count && i < UI_PLAGUE_CITY_TOP_COUNT; i++) {
        draw_city_row(hdc, layout->city_rows[i], &impact->cities[i], i + 1, i & 1);
    }
}
