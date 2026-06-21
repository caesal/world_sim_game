#include "render/panel_alliance.h"

#include "render/panel_alliance_detail.h"
#include "render/panel_alliance_model.h"
#include "render/panel_country_cards.h"
#include "render/render_context.h"
#include "render_panel_internal.h"
#include "ui/ui_clay_primitives.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_theme.h"
#include "ui/ui_widgets.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    RECT header, count_cards[2], sort_label, fallen_toggle, back_to_list, selected_summary;
    RECT sort_columns[COUNTRY_SORT_COUNT];
    RECT detail_tabs[ALLIANCE_DETAIL_TAB_COUNT];
    AllianceMemberSortLayout member_sort;
    RECT rows[ALLIANCE_MAX + MAX_CIVS];
    int row_hits[ALLIANCE_MAX + MAX_CIVS];
    int row_count;
    RECT viewport;
    int scroll, max_scroll;
    int selected_detail;
} AlliancePanelLayout;

static const RenderSnapshot *panel_snapshot(int *owned) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    *owned = 0;
    if (snapshot) return snapshot;
    snapshot = render_snapshot_acquire();
    render_context_begin(snapshot);
    *owned = 1;
    return snapshot;
}

static void panel_snapshot_end(int owned, const RenderSnapshot *snapshot) {
    if (!owned) return;
    render_context_end();
    render_snapshot_release(snapshot);
}

static const char *alliance_name(const RenderSnapshot *snapshot, int alliance_id) {
    const AllianceSnapshotRecord *record = alliance_panel_snapshot_record(snapshot, alliance_id);
    if (!record) return tr("Unknown Alliance", "未知联盟");
    return ui_language == UI_LANG_ZH ? record->name_zh : record->name_en;
}

static const char *status_label(int war_count) {
    return war_count > 0 ? tr("War", "战争") : tr("Peace", "和平");
}

static COLORREF status_color(int war_count) {
    return war_count > 0 ? RGB(112, 58, 52) : RGB(56, 88, 64);
}

static void metric_text(int value, char *out, int out_size) {
    format_metric_value(value, out, out_size);
}

static const char *sort_column_label(int column) {
    static const char *en[COUNTRY_SORT_COUNT] = {"Pop", "Prov", "Army", "Treas", "Tech", "Chaos"};
    static const char *zh[COUNTRY_SORT_COUNT] = {"人口", "省份", "军队", "国库", "科技", "混乱"};
    return tr(en[clamp(column, 0, COUNTRY_SORT_COUNT - 1)],
              zh[clamp(column, 0, COUNTRY_SORT_COUNT - 1)]);
}

static const AlliancePanelModel *panel_model(const RenderSnapshot *snapshot) {
    return alliance_panel_model_get(snapshot, country_show_fallen,
                                    country_sort_column, country_sort_descending);
}

static void draw_badge(HDC hdc, RECT rect, COLORREF color, const char *text) {
    fill_rect(hdc, rect, color);
    draw_text_rect(hdc, rect, text, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_count_cards(HDC hdc, const AlliancePanelLayout *layout,
                             const AlliancePanelModel *model) {
    const char *label0 = tr("Alliances", "联盟");
    const char *label1 = model && model->show_fallen ? tr("Fallen", "已灭亡") :
                         tr("No Alliance", "未结盟国家");
    const char *labels[2] = {label0, label1};
    int values[2] = {model ? model->alliance_row_count : 0,
                     model ? model->no_alliance_country_count : 0};
    int i;
    for (i = 0; i < 2; i++) {
        char text[32];
        RECT r = layout->count_cards[i];
        ui_clay_draw_card(hdc, r, UI_CLAY_STATE_NORMAL);
        draw_text_rect(hdc, (RECT){r.left + 8, r.top + 3, r.right - 8, r.top + 18},
                       labels[i], ui_theme_color(UI_COLOR_TEXT_DIM),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        snprintf(text, sizeof(text), "%d", values[i]);
        draw_text_rect(hdc, (RECT){r.left + 8, r.top + 16, r.right - 8, r.bottom - 3},
                       text, ui_theme_color(UI_COLOR_TEXT),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
}

static void draw_sort_columns(HDC hdc, const AlliancePanelLayout *layout) {
    int i;
    char text[32];
    for (i = 0; i < COUNTRY_SORT_COUNT; i++) {
        RECT rect = layout->sort_columns[i];
        int active = i == country_sort_column;
        UiClayState state = ui_clay_state_for_rect(rect, hover_x, hover_y, active, 0);
        ui_clay_draw_tab(hdc, rect, state);
        snprintf(text, sizeof(text), "%s%s", sort_column_label(i),
                 active ? (country_sort_descending ? " ↓" : " ↑") : "");
        draw_text_rect(hdc, rect, text, ui_clay_text_color(state),
                       DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
    }
}

static void draw_metric(HDC hdc, RECT rect, const char *label, int value, COLORREF color) {
    char text[32];
    metric_text(value, text, sizeof(text));
    draw_text_rect(hdc, (RECT){rect.left, rect.top, rect.right, rect.top + 14},
                   label, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_CENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, (RECT){rect.left, rect.top + 15, rect.right, rect.bottom},
                   text, color, DT_SINGLELINE | DT_CENTER | DT_END_ELLIPSIS);
}

static void draw_alliance_row(HDC hdc, RECT rect, const RenderSnapshot *snapshot,
                              const AlliancePanelRow *row, int selected) {
    RECT swatch = {rect.left + 8, rect.top + 8, rect.left + 24, rect.top + 24};
    RECT name_rect = {rect.left + 32, rect.top + 5, rect.right - 226, rect.top + 27};
    RECT type_rect = {rect.right - 218, rect.top + 6, rect.right - 88, rect.top + 25};
    RECT status_rect = {rect.right - 82, rect.top + 6, rect.right - 8, rect.top + 25};
    int metric_w = (rect.right - rect.left - 16) / 6;
    RECT metric = {rect.left + 8, rect.top + 31, rect.left + 8 + metric_w - 4, rect.top + 68};

    ui_clay_draw_card(hdc, rect, selected ? UI_CLAY_STATE_SELECTED : UI_CLAY_STATE_NORMAL);
    fill_rect(hdc, swatch, (COLORREF)row->color);
    draw_text_rect(hdc, name_rect, alliance_name(snapshot, row->alliance_id),
                   ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_badge(hdc, type_rect, RGB(62, 72, 94), tr("Defensive Alliance", "防御同盟"));
    draw_badge(hdc, status_rect, status_color(row->war_count), status_label(row->war_count));
    draw_metric(hdc, metric, metric_label("Members", "成员"), row->member_count, RGB(190, 204, 216));
    metric.left += metric_w; metric.right += metric_w;
    draw_metric(hdc, metric, metric_label("Pop", "人口"), row->population, ui_theme_color(UI_COLOR_TEXT));
    metric.left += metric_w; metric.right += metric_w;
    draw_metric(hdc, metric, metric_label("Army", "军队"), row->military, RGB(204, 172, 112));
    metric.left += metric_w; metric.right += metric_w;
    draw_metric(hdc, metric, metric_label("Strength", "实力"), row->strength, RGB(178, 198, 142));
    metric.left += metric_w; metric.right += metric_w;
    draw_metric(hdc, metric, metric_label("Tech", "科技"), row->tech_stage, RGB(147, 176, 214));
    metric.left += metric_w; metric.right += metric_w;
    draw_metric(hdc, metric, metric_label("War", "战争"), row->war_count, status_color(row->war_count));
}

static void build_layout(RECT client, AlliancePanelLayout *layout,
                         const RenderSnapshot *snapshot, const AlliancePanelModel *model) {
    int x = client.right - side_panel_w + FORM_X_PAD;
    int width = side_panel_w - FORM_X_PAD * 2;
    int y = TOP_BAR_H + 66;
    const AlliancePanelRow *selected = alliance_panel_model_find_alliance(model, selected_alliance_id);
    int content_h, i, row_y;
    memset(layout, 0, sizeof(*layout));
    layout->selected_detail = selected != NULL;
    layout->header = (RECT){x, y, x + width, y + 24}; y += 25;
    layout->count_cards[0] = (RECT){x, y, x + width / 2 - 4, y + 36};
    layout->count_cards[1] = (RECT){x + width / 2 + 4, y, x + width, y + 36}; y += 44;
    if (selected) {
        int tab_w;
        layout->back_to_list = (RECT){x, y, x + width, y + 26}; y += 34;
        layout->selected_summary = (RECT){x, y, x + width, y + 58}; y += 66;
        tab_w = width / ALLIANCE_DETAIL_TAB_COUNT;
        for (i = 0; i < ALLIANCE_DETAIL_TAB_COUNT; i++)
            layout->detail_tabs[i] = (RECT){x + i * tab_w, y,
                i == ALLIANCE_DETAIL_TAB_COUNT - 1 ? x + width : x + (i + 1) * tab_w - 2, y + 26};
        y += 34;
        if (alliance_detail_subtab == ALLIANCE_DETAIL_MEMBERS) {
            int sort_w = width / ALLIANCE_MEMBER_SORT_COUNT;
            for (i = 0; i < ALLIANCE_MEMBER_SORT_COUNT; i++)
                layout->member_sort.buttons[i] = (RECT){x + i * sort_w, y,
                    i == ALLIANCE_MEMBER_SORT_COUNT - 1 ? x + width : x + (i + 1) * sort_w - 2, y + 26};
            y += 34;
        }
        layout->viewport = (RECT){x, y, x + width, client.bottom - 64};
        content_h = alliance_detail_content_height(snapshot, selected);
        layout->max_scroll = max(0, content_h - (layout->viewport.bottom - layout->viewport.top));
        alliance_detail_subtab = clamp(alliance_detail_subtab, 0, ALLIANCE_DETAIL_TAB_COUNT - 1);
        layout->scroll = clamp(alliance_detail_scroll_offsets[alliance_detail_subtab], 0, layout->max_scroll);
        return;
    }
    layout->sort_label = (RECT){x, y, x + width, y + 20}; y += 25;
    layout->fallen_toggle = (RECT){x, y, x + width, y + 26}; y += 36;
    {
        int column_w = width / COUNTRY_SORT_COUNT;
        for (i = 0; i < COUNTRY_SORT_COUNT; i++)
            layout->sort_columns[i] = (RECT){x + i * column_w, y,
                i == COUNTRY_SORT_COUNT - 1 ? x + width : x + (i + 1) * column_w - 4, y + 26};
    }
    y += 34;
    layout->viewport = (RECT){x, y, x + width, client.bottom - 64};
    content_h = model ? model->row_count * 80 : 0;
    layout->max_scroll = max(0, content_h - (layout->viewport.bottom - layout->viewport.top));
    layout->scroll = clamp(alliance_list_scroll_offset, 0, layout->max_scroll);
    row_y = y - layout->scroll;
    for (i = 0; model && i < model->row_count && layout->row_count < ALLIANCE_MAX + MAX_CIVS; i++) {
        RECT row = {x, row_y, x + width, row_y + 72};
        row_y += 80;
        if (row.bottom < layout->viewport.top || row.top > layout->viewport.bottom) continue;
        layout->rows[layout->row_count] = row;
        layout->row_hits[layout->row_count] = model->rows[i].kind == ALLIANCE_PANEL_ROW_ALLIANCE ?
            ALLIANCE_PANEL_HIT_ALLIANCE_BASE + model->rows[i].alliance_id :
            ALLIANCE_PANEL_HIT_COUNTRY_BASE + model->rows[i].civ_id;
        layout->row_count++;
    }
}

static void draw_detail_tabs(HDC hdc, const AlliancePanelLayout *layout) {
    int i;
    for (i = 0; i < ALLIANCE_DETAIL_TAB_COUNT; i++) {
        UiClayState state = ui_clay_state_for_rect(layout->detail_tabs[i], hover_x, hover_y,
                                                   i == alliance_detail_subtab, 0);
        ui_clay_draw_tab(hdc, layout->detail_tabs[i], state);
        draw_text_rect(hdc, layout->detail_tabs[i], alliance_detail_tab_label(i), ui_clay_text_color(state),
                       DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
    }
}

static void draw_summary(HDC hdc, RECT rect, const RenderSnapshot *snapshot,
                         const AlliancePanelRow *row) {
    char text[256], pop[32], mil[32];
    RECT swatch = {rect.left + 8, rect.top + 8, rect.left + 26, rect.top + 26};
    RECT name_rect = {rect.left + 34, rect.top + 5, rect.right - 8, rect.top + 28};
    RECT summary = {rect.left + 34, rect.top + 30, rect.right - 8, rect.bottom - 4};
    metric_text(row->population, pop, sizeof(pop));
    metric_text(row->military, mil, sizeof(mil));
    ui_clay_draw_card(hdc, rect, UI_CLAY_STATE_SELECTED);
    fill_rect(hdc, swatch, (COLORREF)row->color);
    draw_text_rect(hdc, name_rect, alliance_name(snapshot, row->alliance_id),
                   ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%s | %s %s | %s %d | %s %d | %s %s | %s %s",
             tr("Defensive Alliance", "防御同盟"), tr("Status", "状态"), status_label(row->war_count),
             tr("Members", "成员"), row->member_count, tr("Vassals", "附庸"), row->vassal_count,
             tr("Pop", "人口"), pop, tr("Army", "军队"), mil);
    draw_text_rect(hdc, summary, text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_scrollbar(HDC hdc, RECT viewport, int scroll, int max_scroll) {
    RECT track = {viewport.right - 5, viewport.top, viewport.right - 2, viewport.bottom};
    RECT thumb = track;
    int height = viewport.bottom - viewport.top, thumb_h;
    if (max_scroll <= 0 || height <= 0) return;
    thumb_h = max(28, height * height / max(1, height + max_scroll));
    thumb.top = viewport.top + scroll * (height - thumb_h) / max_scroll;
    thumb.bottom = thumb.top + thumb_h;
    fill_rect(hdc, track, RGB(40, 47, 52));
    fill_rect(hdc, thumb, RGB(115, 130, 138));
}

void draw_alliance_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    int owned = 0, i, row_index = 0;
    const RenderSnapshot *snapshot = panel_snapshot(&owned);
    const AlliancePanelModel *model = panel_model(snapshot);
    const AlliancePanelRow *selected = alliance_panel_model_find_alliance(model, selected_alliance_id);
    AlliancePanelLayout layout;
    (void)x; (void)title_font;
    SelectObject(hdc, body_font);
    build_layout(client, &layout, snapshot, model);
    draw_text_rect(hdc, layout.header, tr("Alliance Dashboard", "联盟面板"),
                   ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_count_cards(hdc, &layout, model);
    if (!layout.selected_detail) {
        char text[96];
        HRGN clip;
        snprintf(text, sizeof(text), "%s  %s %s", tr("Sort", "排序"),
                 sort_column_label(country_sort_column),
                 country_sort_descending ? tr("desc", "降序") : tr("asc", "升序"));
        draw_text_rect(hdc, layout.sort_label, text, ui_theme_color(UI_COLOR_TEXT_DIM),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        ui_clay_draw_pill_button(hdc, layout.fallen_toggle,
                                 country_show_fallen ? tr("Show Active Countries", "显示活跃国家") :
                                                       tr("Show Fallen Countries", "显示已灭亡国家"),
                                 ui_clay_state_for_rect(layout.fallen_toggle, hover_x, hover_y,
                                                        country_show_fallen, 0));
        draw_sort_columns(hdc, &layout);
        clip = CreateRectRgn(layout.viewport.left, layout.viewport.top,
                             layout.viewport.right, layout.viewport.bottom);
        SelectClipRgn(hdc, clip);
        for (i = 0; model && i < model->row_count; i++) {
            const AlliancePanelRow *row = &model->rows[i];
            if (row_index >= layout.row_count) break;
            if (layout.row_hits[row_index] == (row->kind == ALLIANCE_PANEL_ROW_ALLIANCE ?
                ALLIANCE_PANEL_HIT_ALLIANCE_BASE + row->alliance_id :
                ALLIANCE_PANEL_HIT_COUNTRY_BASE + row->civ_id)) {
                if (row->kind == ALLIANCE_PANEL_ROW_ALLIANCE)
                    draw_alliance_row(hdc, layout.rows[row_index], snapshot, row, 0);
                else draw_country_summary_card(hdc, layout.rows[row_index], row->civ_id, row->civ_id == selected_civ);
                row_index++;
            }
        }
        SelectClipRgn(hdc, NULL);
        DeleteObject(clip);
        draw_scrollbar(hdc, layout.viewport, layout.scroll, layout.max_scroll);
        if (!model || model->row_count == 0)
            draw_text_rect(hdc, layout.viewport, tr("No alliances or countries to show.", "暂无联盟或国家。"),
                           ui_theme_color(UI_COLOR_TEXT_MUTED), DT_WORDBREAK | DT_END_ELLIPSIS);
        panel_snapshot_end(owned, snapshot);
        return;
    }
    ui_clay_draw_pill_button(hdc, layout.back_to_list, tr("All Alliances / Back to list", "全部联盟 / 返回列表"),
                             ui_clay_state_for_rect(layout.back_to_list, hover_x, hover_y, 0, 0));
    draw_summary(hdc, layout.selected_summary, snapshot, selected);
    draw_detail_tabs(hdc, &layout);
    if (alliance_detail_subtab == ALLIANCE_DETAIL_MEMBERS)
        alliance_detail_draw_member_sort(hdc, &layout.member_sort);
    {
        HRGN clip = CreateRectRgn(layout.viewport.left, layout.viewport.top,
                                  layout.viewport.right, layout.viewport.bottom);
        UiCursor cursor = ui_cursor(layout.viewport.left, layout.viewport.top - layout.scroll,
                                    layout.viewport.right - layout.viewport.left - 8,
                                    layout.viewport.bottom - layout.scroll + 400);
        SelectClipRgn(hdc, clip);
        alliance_detail_draw_content(hdc, &cursor, snapshot, selected);
        SelectClipRgn(hdc, NULL);
        DeleteObject(clip);
        draw_scrollbar(hdc, layout.viewport, layout.scroll, layout.max_scroll);
    }
    panel_snapshot_end(owned, snapshot);
}

int alliance_panel_hit_test(RECT client, int mouse_x, int mouse_y) {
    int owned = 0, i;
    const RenderSnapshot *snapshot = panel_snapshot(&owned);
    const AlliancePanelModel *model = panel_model(snapshot);
    AlliancePanelLayout layout;
    build_layout(client, &layout, snapshot, model);
    if (layout.selected_detail && point_in_rect_local(layout.back_to_list, mouse_x, mouse_y)) {
        panel_snapshot_end(owned, snapshot);
        return ALLIANCE_PANEL_HIT_BACK_TO_LIST;
    }
    if (!layout.selected_detail && point_in_rect_local(layout.fallen_toggle, mouse_x, mouse_y)) {
        panel_snapshot_end(owned, snapshot);
        return ALLIANCE_PANEL_HIT_TOGGLE_FALLEN;
    }
    if (!layout.selected_detail) {
        for (i = 0; i < COUNTRY_SORT_COUNT; i++) {
            if (point_in_rect_local(layout.sort_columns[i], mouse_x, mouse_y)) {
                panel_snapshot_end(owned, snapshot);
                return ALLIANCE_PANEL_HIT_SORT_POPULATION - i;
            }
        }
    }
    if (layout.selected_detail) {
        for (i = 0; i < ALLIANCE_DETAIL_TAB_COUNT; i++) {
            if (point_in_rect_local(layout.detail_tabs[i], mouse_x, mouse_y)) {
                panel_snapshot_end(owned, snapshot);
                return ALLIANCE_PANEL_HIT_SUBTAB_BASE - i;
            }
        }
        if (alliance_detail_subtab == ALLIANCE_DETAIL_MEMBERS) {
            for (i = 0; i < ALLIANCE_MEMBER_SORT_COUNT; i++) {
                if (point_in_rect_local(layout.member_sort.buttons[i], mouse_x, mouse_y)) {
                    panel_snapshot_end(owned, snapshot);
                    return ALLIANCE_PANEL_HIT_MEMBER_SORT_BASE - i;
                }
            }
        }
    }
    for (i = 0; i < layout.row_count; i++) {
        if (point_in_rect_local(layout.rows[i], mouse_x, mouse_y)) {
            int hit = layout.row_hits[i];
            panel_snapshot_end(owned, snapshot);
            return hit;
        }
    }
    panel_snapshot_end(owned, snapshot);
    return ALLIANCE_PANEL_HIT_NONE;
}

int alliance_panel_scroll(RECT client, int delta) {
    int owned = 0, changed = 0;
    const RenderSnapshot *snapshot = panel_snapshot(&owned);
    const AlliancePanelModel *model = panel_model(snapshot);
    AlliancePanelLayout layout;
    build_layout(client, &layout, snapshot, model);
    if (layout.max_scroll > 0) {
        if (layout.selected_detail) {
            int tab = clamp(alliance_detail_subtab, 0, ALLIANCE_DETAIL_TAB_COUNT - 1);
            int old = alliance_detail_scroll_offsets[tab];
            alliance_detail_scroll_offsets[tab] = clamp(old + delta, 0, layout.max_scroll);
            alliance_detail_scroll_offset = alliance_detail_scroll_offsets[tab];
            changed = alliance_detail_scroll_offsets[tab] != old;
        } else {
            int old = alliance_list_scroll_offset;
            alliance_list_scroll_offset = clamp(old + delta, 0, layout.max_scroll);
            changed = alliance_list_scroll_offset != old;
        }
    }
    panel_snapshot_end(owned, snapshot);
    return changed;
}
