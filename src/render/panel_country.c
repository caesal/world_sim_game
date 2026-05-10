#include "render_panel_internal.h"
#include "render/panel_country.h"
#include "render/panel_country_detail.h"

#include "sim/collapse.h"
#include "sim/decision_snapshot.h"
#include "sim/expansion.h"
#include "sim/plague.h"
#include "sim/population.h"
#include "sim/regions.h"
#include "sim/technology.h"
#include "sim/vassal.h"
#include "ui/ui_widgets.h"

#include <string.h>

typedef struct {
    RECT header;
    RECT sort_label;
    RECT fallen_toggle;
    RECT sort_columns[COUNTRY_SORT_COUNT];
    RECT back_to_list;
    RECT selected_summary;
    RECT detail_tabs[COUNTRY_DETAIL_TAB_COUNT];
    RECT cards[MAX_CIVS];
    int card_civ_ids[MAX_CIVS];
    int card_count;
    RECT list_viewport;
    int list_scroll;
    int list_max_scroll;
    RECT detail_viewport;
    int detail_scroll;
    int detail_max_scroll;
    int selected_detail;
} CountryPanelLayout;

typedef struct {
    int civ_id;
    int depth;
} CountryListEntry;

static int displayed_country(void) {
    if (selected_civ >= 0 && selected_civ < civ_count &&
        (civs[selected_civ].alive || country_show_fallen)) return selected_civ;
    return -1;
}

static int country_province_count(int civ_id) {
    int i;
    int count = 0;
    for (i = 0; i < region_count; i++) {
        if (natural_regions[i].alive && natural_regions[i].owner_civ == civ_id) count++;
    }
    return count > 0 ? count : summarize_country(civ_id).cities;
}

static COLORREF disorder_color(int disorder) {
    if (disorder <= 30) return RGB(104, 153, 92);
    if (disorder <= 65) return RGB(191, 154, 78);
    return RGB(188, 78, 68);
}

static const char *country_intent_label(const char *intent) {
    if (!intent) return tr("Unknown", "未知");
    if (strcmp(intent, "War") == 0) return tr("War", "战争");
    if (strcmp(intent, "Stability") == 0) return tr("Stability", "稳定");
    return tr("Expansion", "扩张");
}

static void draw_scrollbar(HDC hdc, RECT viewport, int scroll, int max_scroll);

static int include_country_in_list(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && (civs[civ_id].alive || country_show_fallen);
}

static int country_sort_value(int civ_id, int column) {
    switch (clamp(column, 0, COUNTRY_SORT_COUNT - 1)) {
        case COUNTRY_SORT_PROVINCES: return country_province_count(civ_id);
        case COUNTRY_SORT_ARMY: return war_current_soldiers_for_civ(civ_id);
        case COUNTRY_SORT_TECH: return clamp(civs[civ_id].tech_stage, 0, 10);
        case COUNTRY_SORT_DISORDER: return civs[civ_id].disorder;
        default: return summarize_country(civ_id).population;
    }
}

static void sort_country_ids(int *ids, int *count) {
    int i;
    int j;

    for (i = 1; i < *count; i++) {
        int id = ids[i];
        int value = country_sort_value(id, country_sort_column);
        j = i - 1;
        while (j >= 0) {
            int other = ids[j];
            int other_value = country_sort_value(other, country_sort_column);
            int move = country_sort_descending ? other_value < value : other_value > value;
            if (!move && other_value == value) {
                move = country_sort_descending ? civs[other].population < civs[id].population :
                                                 civs[other].population > civs[id].population;
                if (!move && civs[other].population == civs[id].population) move = other > id;
            }
            if (!move) break;
            ids[j + 1] = ids[j];
            j--;
        }
        ids[j + 1] = id;
    }
}

static void append_vassal_tree(int root, int depth, int *used,
                               CountryListEntry *entries, int *entry_count) {
    int children[MAX_CIVS];
    int child_count = 0;
    int i;

    if (*entry_count >= MAX_CIVS) return;
    entries[*entry_count].civ_id = root;
    entries[*entry_count].depth = clamp(depth, 0, 1);
    (*entry_count)++;
    used[root] = 1;
    for (i = 0; i < civ_count; i++) {
        if (used[i] || !include_country_in_list(i)) continue;
        if (vassal_is_direct(root, i)) children[child_count++] = i;
    }
    sort_country_ids(children, &child_count);
    for (i = 0; i < child_count && *entry_count < MAX_CIVS; i++) {
        entries[*entry_count].civ_id = children[i];
        entries[*entry_count].depth = 1;
        (*entry_count)++;
        used[children[i]] = 1;
    }
}

static void country_list_entries(CountryListEntry *entries, int *out_count) {
    int roots[MAX_CIVS];
    int used[MAX_CIVS];
    int root_count = 0;
    int i;

    memset(used, 0, sizeof(used));
    for (i = 0; i < civ_count; i++) {
        if (!include_country_in_list(i)) continue;
        if (vassal_overlord(i) < 0) roots[root_count++] = i;
    }
    sort_country_ids(roots, &root_count);
    *out_count = 0;
    for (i = 0; i < root_count; i++) append_vassal_tree(roots[i], 0, used, entries, out_count);
    for (i = 0; i < civ_count && *out_count < MAX_CIVS; i++) {
        if (include_country_in_list(i) && !used[i]) append_vassal_tree(i, 0, used, entries, out_count);
    }
}

static void country_panel_layout_build(RECT client, CountryPanelLayout *layout) {
    int x = client.right - side_panel_w + FORM_X_PAD;
    int width = side_panel_w - FORM_X_PAD * 2;
    int y = TOP_BAR_H + 66;
    int civ_id = displayed_country();
    CountryListEntry entries[MAX_CIVS];
    int count;
    int i;
    int card_y;
    int content_h;

    memset(layout, 0, sizeof(*layout));
    layout->selected_detail = civ_id >= 0 && civ_id < civ_count;
    layout->header = (RECT){x, y, x + width, y + 24};
    y += 25;
    layout->sort_label = (RECT){x, y, x + width, y + 20};
    y += 25;
    layout->fallen_toggle = (RECT){x, y, x + width, y + 26};
    y += 36;
    if (layout->selected_detail) {
        int tab_w;
        layout->back_to_list = (RECT){x, y, x + width, y + 26};
        y += 34;
        layout->selected_summary = (RECT){x, y, x + width, y + 50};
        y += 60;
        tab_w = width / COUNTRY_DETAIL_TAB_COUNT;
        for (i = 0; i < COUNTRY_DETAIL_TAB_COUNT; i++) {
            layout->detail_tabs[i] = (RECT){x + i * tab_w, y,
                                            i == COUNTRY_DETAIL_TAB_COUNT - 1 ? x + width : x + (i + 1) * tab_w - 2,
                                            y + 26};
        }
        y += 34;
        layout->detail_viewport = (RECT){x, y, x + width, client.bottom - 64};
        layout->detail_max_scroll = max(0, country_detail_content_height(civ_id) -
                                           (layout->detail_viewport.bottom - layout->detail_viewport.top));
        country_detail_subtab = clamp(country_detail_subtab, 0, COUNTRY_DETAIL_TAB_COUNT - 1);
        layout->detail_scroll = clamp(country_detail_scroll_offsets[country_detail_subtab], 0,
                                      layout->detail_max_scroll);
        return;
    }
    country_list_entries(entries, &count);
    {
        int column_w = width / COUNTRY_SORT_COUNT;
        for (i = 0; i < COUNTRY_SORT_COUNT; i++) {
            layout->sort_columns[i] = (RECT){x + i * column_w, y, i == COUNTRY_SORT_COUNT - 1 ? x + width : x + (i + 1) * column_w - 4, y + 26};
        }
    }
    y += 34;
    layout->list_viewport = (RECT){x, y, x + width, client.bottom - 64};
    content_h = count * 80;
    layout->list_max_scroll = max(0, content_h - (layout->list_viewport.bottom - layout->list_viewport.top));
    layout->list_scroll = clamp(country_list_scroll_offset, 0, layout->list_max_scroll);
    card_y = y - layout->list_scroll;
    for (i = 0; i < count && layout->card_count < MAX_CIVS; i++) {
        int indent = clamp(entries[i].depth, 0, 3) * 18;
        RECT card = {x + indent, card_y, x + width, card_y + 72};
        card_y += 80;
        if (card.bottom < layout->list_viewport.top || card.top > layout->list_viewport.bottom) continue;
        layout->cards[layout->card_count] = card;
        layout->card_civ_ids[layout->card_count] = entries[i].civ_id;
        layout->card_count++;
    }
    layout->detail_viewport = layout->list_viewport;
}

static void draw_card_metric(HDC hdc, RECT rect, const char *label, int value, COLORREF color) {
    char text[32];
    RECT label_rect = {rect.left, rect.top, rect.right, rect.top + 15};
    RECT value_rect = {rect.left, rect.top + 16, rect.right, rect.top + 38};

    format_metric_value(value, text, sizeof(text));
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_CENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, value_rect, text, color, DT_SINGLELINE | DT_CENTER | DT_END_ELLIPSIS);
}

static void draw_country_card(HDC hdc, RECT rect, int civ_id) {
    RECT swatch = {rect.left + 8, rect.top + 8, rect.left + 24, rect.top + 24};
    RECT name_rect = {rect.left + 32, rect.top + 5, rect.right - 8, rect.top + 27};
    int metric_w = (rect.right - rect.left - 16) / 5;
    RECT metric = {rect.left + 8, rect.top + 30, rect.left + 8 + metric_w - 4, rect.top + 68};
    CountrySummary country = summarize_country(civ_id);
    char title[160];
    char prefix[16] = "";
    COLORREF text_color = civs[civ_id].alive ? ui_theme_color(UI_COLOR_TEXT) : ui_theme_color(UI_COLOR_TEXT_DIM);

    fill_rect(hdc, rect, civ_id == selected_civ ? RGB(54, 61, 58) : ui_theme_color(UI_COLOR_PANEL_SOFT));
    fill_rect(hdc, swatch, civs[civ_id].color);
    if (vassal_overlord(civ_id) >= 0) snprintf(prefix, sizeof(prefix), "sub ");
    snprintf(title, sizeof(title), "%s%c  %.80s%s", prefix, civs[civ_id].symbol, civilization_display_name(civ_id),
             civs[civ_id].alive ? "" : tr(" (fallen)", "（已灭亡）"));
    draw_text_rect(hdc, name_rect, title, text_color, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_card_metric(hdc, metric, metric_label("Pop", "人口"), country.population, text_color);
    metric.left += metric_w; metric.right += metric_w;
    draw_card_metric(hdc, metric, metric_label("Provinces", "省份"), country_province_count(civ_id), RGB(190, 204, 216));
    metric.left += metric_w; metric.right += metric_w;
    draw_card_metric(hdc, metric, metric_label("Army", "军队"), war_current_soldiers_for_civ(civ_id), RGB(204, 172, 112));
    metric.left += metric_w; metric.right += metric_w;
    draw_card_metric(hdc, metric, metric_label("Tech", "科技"), clamp(civs[civ_id].tech_stage, 0, 10), RGB(147, 176, 214));
    metric.left += metric_w; metric.right += metric_w;
    draw_card_metric(hdc, metric, metric_label("Disorder", "混乱"), civs[civ_id].disorder, disorder_color(civs[civ_id].disorder));
}

static void draw_country_selector(HDC hdc, RECT rect, int civ_id) {
    RECT swatch = {rect.left + 8, rect.top + 8, rect.left + 24, rect.top + 24};
    RECT name_rect = {rect.left + 32, rect.top + 5, rect.right - 8, rect.top + 26};
    RECT summary_rect = {rect.left + 32, rect.top + 27, rect.right - 8, rect.bottom - 4};
    CountrySummary country = summarize_country(civ_id);
    DecisionSnapshot decision;
    char title[160];
    char summary[192];

    decision_snapshot_for_civ(civ_id, &decision);
    fill_rect(hdc, rect, RGB(54, 61, 58));
    fill_rect(hdc, swatch, civs[civ_id].color);
    snprintf(title, sizeof(title), "%c  %.80s%s", civs[civ_id].symbol, civilization_display_name(civ_id),
             civs[civ_id].alive ? "" : tr(" (fallen)", "（已灭亡）"));
    snprintf(summary, sizeof(summary), "%s %.24s   %s %d   %s %d   %s %d   %s %d   %s %.16s",
             tr("Capital", "首都"), capital_name_for_civ(civ_id),
             tr("Pop", "人口"), country.population, tr("Army", "军队"), war_current_soldiers_for_civ(civ_id),
             tr("Tech", "科技"), clamp(civs[civ_id].tech_stage, 0, 10),
             tr("Disorder", "混乱"), civs[civ_id].disorder, tr("Intent", "意图"),
             country_intent_label(decision.main_intent));
    draw_text_rect(hdc, name_rect, title, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, summary_rect, summary, ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

static const char *sort_column_label(int column) {
    static const char *labels_en[COUNTRY_SORT_COUNT] = {"Pop", "Prov", "Army", "Tech", "Chaos"};
    static const char *labels_zh[COUNTRY_SORT_COUNT] = {"人口", "省份", "军队", "科技", "混乱"};
    column = clamp(column, 0, COUNTRY_SORT_COUNT - 1);
    return tr(labels_en[column], labels_zh[column]);
}

static const char *country_detail_tab_label(int tab) {
    static const char *en[COUNTRY_DETAIL_TAB_COUNT] = {
        "Overview", "Technology", "Decision", "Population", "Resources", "Diplomacy", "Disorder"
    };
    static const char *zh[COUNTRY_DETAIL_TAB_COUNT] = {
        "总览", "科技", "决策", "人口", "资源", "外交", "混乱"
    };
    tab = clamp(tab, 0, COUNTRY_DETAIL_TAB_COUNT - 1);
    return tr(en[tab], zh[tab]);
}

static void draw_detail_tabs(HDC hdc, const CountryPanelLayout *layout) {
    int i;
    for (i = 0; i < COUNTRY_DETAIL_TAB_COUNT; i++) {
        int active = i == country_detail_subtab;
        fill_rect(hdc, layout->detail_tabs[i], active ? RGB(87, 93, 78) : RGB(43, 49, 52));
        draw_center_text(hdc, layout->detail_tabs[i], country_detail_tab_label(i),
                         active ? RGB(255, 238, 190) : ui_theme_color(UI_COLOR_TEXT));
    }
}

static void draw_sort_columns(HDC hdc, const CountryPanelLayout *layout) {
    int i;
    char text[32];

    for (i = 0; i < COUNTRY_SORT_COUNT; i++) {
        RECT rect = layout->sort_columns[i];
        int active = i == country_sort_column;
        fill_rect(hdc, rect, active ? RGB(87, 93, 78) : RGB(43, 49, 52));
        snprintf(text, sizeof(text), "%s%s", sort_column_label(i),
                 active ? (country_sort_descending ? " ↓" : " ↑") : "");
        draw_center_text(hdc, rect, text, active ? RGB(255, 238, 190) : ui_theme_color(UI_COLOR_TEXT));
    }
}

static void draw_country_list(HDC hdc, const CountryPanelLayout *layout) {
    int i;
    char text[96];

    draw_text_rect(hdc, layout->header, tr("Country Dashboard", "国家面板"),
                   ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%s  %s %s", tr("Sort", "排序"),
             sort_column_label(country_sort_column),
             country_sort_descending ? tr("desc", "降序") : tr("asc", "升序"));
    draw_text_rect(hdc, layout->sort_label, text, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    fill_rect(hdc, layout->fallen_toggle, country_show_fallen ? RGB(87, 93, 78) : RGB(43, 49, 52));
    draw_center_text(hdc, layout->fallen_toggle, tr("Show Fallen Countries", "显示已灭亡国家"), ui_theme_color(UI_COLOR_TEXT));
    if (layout->selected_detail) {
        fill_rect(hdc, layout->back_to_list, RGB(43, 49, 52));
        draw_center_text(hdc, layout->back_to_list, tr("All Countries / Back to list", "全部国家 / 返回列表"),
                         ui_theme_color(UI_COLOR_TEXT));
        draw_country_selector(hdc, layout->selected_summary, displayed_country());
        draw_detail_tabs(hdc, layout);
        return;
    }
    draw_sort_columns(hdc, layout);
    {
        HRGN clip = CreateRectRgn(layout->list_viewport.left, layout->list_viewport.top,
                                  layout->list_viewport.right, layout->list_viewport.bottom);
        SelectClipRgn(hdc, clip);
        for (i = 0; i < layout->card_count; i++) {
            draw_country_card(hdc, layout->cards[i], layout->card_civ_ids[i]);
        }
        SelectClipRgn(hdc, NULL);
        DeleteObject(clip);
    }
    draw_scrollbar(hdc, layout->list_viewport, layout->list_scroll, layout->list_max_scroll);
    if (layout->card_count == 0) {
        draw_text_rect(hdc, layout->detail_viewport,
                       tr("No alive countries. Toggle fallen countries for historical entries.",
                          "没有存活国家。可切换显示已灭亡国家查看历史条目。"),
                       ui_theme_color(UI_COLOR_TEXT_MUTED), DT_WORDBREAK | DT_END_ELLIPSIS);
    }
}

static void draw_scrollbar(HDC hdc, RECT viewport, int scroll, int max_scroll) {
    RECT track = {viewport.right - 5, viewport.top, viewport.right - 2, viewport.bottom};
    RECT thumb = track;
    int height = viewport.bottom - viewport.top;
    int thumb_h;

    if (max_scroll <= 0 || height <= 0) return;
    thumb_h = max(28, height * height / max(1, height + max_scroll));
    thumb.top = viewport.top + scroll * (height - thumb_h) / max_scroll;
    thumb.bottom = thumb.top + thumb_h;
    fill_rect(hdc, track, RGB(40, 47, 52));
    fill_rect(hdc, thumb, RGB(115, 130, 138));
}

void draw_country_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    CountryPanelLayout layout;
    int civ_id = displayed_country();
    (void)x;

    SelectObject(hdc, body_font);
    country_detail_reset_hit();
    country_panel_layout_build(client, &layout);
    draw_country_list(hdc, &layout);
    if (civ_id < 0 || civ_id >= civ_count) return;
    {
        HRGN clip = CreateRectRgn(layout.detail_viewport.left, layout.detail_viewport.top,
                                  layout.detail_viewport.right, layout.detail_viewport.bottom);
        UiCursor cursor = ui_cursor(layout.detail_viewport.left, layout.detail_viewport.top - layout.detail_scroll,
                                    layout.detail_viewport.right - layout.detail_viewport.left - 8,
                                    layout.detail_viewport.top - layout.detail_scroll +
                                    country_detail_content_height(civ_id) + 80);
        SelectClipRgn(hdc, clip);
        draw_country_detail_content(hdc, &cursor, civ_id, title_font, body_font);
        SelectClipRgn(hdc, NULL);
        DeleteObject(clip);
        draw_scrollbar(hdc, layout.detail_viewport, layout.detail_scroll, layout.detail_max_scroll);
    }
}

int country_panel_hit_test(RECT client, int mouse_x, int mouse_y) {
    CountryPanelLayout layout;
    int i;

    country_panel_layout_build(client, &layout);
    if (point_in_rect_local(layout.fallen_toggle, mouse_x, mouse_y)) return COUNTRY_PANEL_HIT_TOGGLE_FALLEN;
    if (!layout.selected_detail) {
        for (i = 0; i < COUNTRY_SORT_COUNT; i++) {
            if (point_in_rect_local(layout.sort_columns[i], mouse_x, mouse_y)) {
                return COUNTRY_PANEL_HIT_SORT_POPULATION - i;
            }
        }
    } else {
        for (i = 0; i < COUNTRY_DETAIL_TAB_COUNT; i++) {
            if (point_in_rect_local(layout.detail_tabs[i], mouse_x, mouse_y)) {
                return COUNTRY_PANEL_HIT_SUBTAB_BASE - i;
            }
        }
    }
    if (layout.selected_detail && point_in_rect_local(layout.back_to_list, mouse_x, mouse_y)) {
        return COUNTRY_PANEL_HIT_BACK_TO_LIST;
    }
    if (country_detail_civil_unrest_hit(layout.detail_viewport, mouse_x, mouse_y)) {
        return COUNTRY_PANEL_HIT_CIVIL_UNREST;
    }
    if (layout.selected_detail && point_in_rect_local(layout.selected_summary, mouse_x, mouse_y)) return displayed_country();
    for (i = 0; i < layout.card_count; i++) {
        if (point_in_rect_local(layout.cards[i], mouse_x, mouse_y)) return layout.card_civ_ids[i];
    }
    return COUNTRY_PANEL_HIT_NONE;
}

int country_panel_scroll(RECT client, int delta) {
    CountryPanelLayout layout;

    country_panel_layout_build(client, &layout);
    if (layout.selected_detail) {
        int old_offset;
        if (layout.detail_max_scroll <= 0) return 0;
        country_detail_subtab = clamp(country_detail_subtab, 0, COUNTRY_DETAIL_TAB_COUNT - 1);
        old_offset = country_detail_scroll_offsets[country_detail_subtab];
        country_detail_scroll_offsets[country_detail_subtab] =
            clamp(country_detail_scroll_offsets[country_detail_subtab] + delta, 0, layout.detail_max_scroll);
        country_detail_scroll_offset = country_detail_scroll_offsets[country_detail_subtab];
        return country_detail_scroll_offsets[country_detail_subtab] != old_offset;
    } else {
        int old_offset = country_list_scroll_offset;
        if (layout.list_max_scroll <= 0) return 0;
        country_list_scroll_offset = clamp(country_list_scroll_offset + delta, 0, layout.list_max_scroll);
        return country_list_scroll_offset != old_offset;
    }
}
