#include "panel_country_tech.h"

#include "render/render_common.h"
#include "render/ui_format.h"
#include "sim/disorder.h"
#include "sim/population.h"
#include "sim/simulation.h"
#include "sim/technology.h"
#include "ui/ui_widgets.h"

#include <stdio.h>
#include <string.h>

static void fmt_mult(char *buffer, size_t size, int percent) {
    snprintf(buffer, size, "x%d.%02d", percent / 100, percent % 100);
}

static int hovered_stage_node(RECT track, int current_stage) {
    int i;
    for (i = 0; i <= 10; i++) {
        int x = track.left + (track.right - track.left) * i / 10;
        RECT node = {x - 8, track.top - 8, x + 8, track.top + 8};
        if (point_in_rect_local(node, hover_x, hover_y)) return i;
    }
    return current_stage;
}

static void draw_stage_track(HDC hdc, RECT track, int stage, int progress) {
    RECT rail = {track.left, track.top - 2, track.right, track.top + 2};
    RECT fill = rail;
    int fill_right = track.left + (track.right - track.left) * stage / 10;
    int i;
    if (stage < 10) fill_right += (track.right - track.left) * progress / 100 / 10;
    fill.right = fill_right;
    fill_rect(hdc, rail, RGB(52, 61, 65));
    fill_rect(hdc, fill, RGB(204, 168, 74));
    for (i = 0; i <= 10; i++) {
        int x = track.left + (track.right - track.left) * i / 10;
        RECT dot = {x - 5, track.top - 5, x + 5, track.top + 5};
        COLORREF color = i < stage ? RGB(147, 168, 113) :
                         (i == stage ? RGB(232, 196, 88) : RGB(70, 78, 82));
        fill_rect(hdc, dot, color);
    }
}

static void bonus_text(int index, int stage, TechnologyBonusSummary bonus, char *value, size_t value_size) {
    switch (index) {
        case 0: fmt_mult(value, value_size, bonus.expansion_percent); break;
        case 1: fmt_mult(value, value_size, bonus.resource_percent); break;
        case 2:
            if (stage < 6) snprintf(value, value_size, "%s", tr("Locked", "未解锁"));
            else snprintf(value, value_size, "%d%%", 100 - clamp(bonus.deep_sea_stability, 0, 100));
            break;
        case 3: snprintf(value, value_size, "+%d%%", bonus.battle_chance_bonus); break;
        case 4: fmt_mult(value, value_size, bonus.technology_percent); break;
        case 5: fmt_mult(value, value_size, bonus.defense_army_percent); break;
        default:
            snprintf(value, value_size, "%s",
                     bonus.vassal_annexation_unlocked ? tr("Unlocked", "已解锁") : tr("Locked", "未解锁"));
            break;
    }
}

static const char *bonus_label(int index, int stage) {
    static const char *en[] = {"Expansion Bonus", "Resource Bonus", "Deep Sea Sailing", "Battle Bonus",
                              "Tech Progress", "Defense Bonus", "Vassal Annexation"};
    static const char *zh[] = {"扩张加成", "资源加成", "深海航行", "战斗加成", "科技推进", "防御加成", "附庸吞并"};
    if (index == 2 && stage >= 6) return tr("Deep Sea Mortality", "深海死亡率");
    return tr(en[index], zh[index]);
}

static int bonus_is_base(int index, TechnologyBonusSummary bonus) {
    switch (index) {
        case 0: return bonus.expansion_percent <= 100;
        case 1: return bonus.resource_percent <= 100;
        case 2: return bonus.deep_sea_stability <= 0;
        case 3: return bonus.battle_chance_bonus <= 0;
        case 4: return bonus.technology_percent <= 100;
        case 5: return bonus.defense_army_percent <= 100;
        default: return !bonus.vassal_annexation_unlocked;
    }
}

static int bonus_is_unlock_type(int index) {
    return index == 2 || index == 6;
}

static int bonus_changed(int index, int stage, TechnologyBonusSummary now, TechnologyBonusSummary next) {
    char a[32], b[32];
    bonus_text(index, stage, now, a, sizeof(a));
    bonus_text(index, min(stage + 1, 10), next, b, sizeof(b));
    return strcmp(a, b) != 0;
}

static COLORREF bonus_function_color(int index) {
    switch (index) {
        case 0: return RGB(72, 160, 128);
        case 1: return RGB(204, 168, 74);
        case 2: return RGB(70, 166, 182);
        case 3: return RGB(184, 78, 68);
        case 4: return RGB(104, 158, 206);
        case 5: return RGB(112, 136, 166);
        default: return RGB(154, 105, 178);
    }
}

static int bonus_locked(int index, int stage, TechnologyBonusSummary now) {
    if (index == 2) return stage < 6;
    if (index == 6) return !now.vassal_annexation_unlocked;
    return 0;
}

static void draw_bonus_cell(HDC hdc, RECT rect, const char *label, const char *value,
                            const char *state, COLORREF accent) {
    RECT stripe = {rect.left, rect.top, rect.left + 4, rect.bottom};
    RECT label_rect = {rect.left + 10, rect.top + 4, rect.right - 8, rect.top + 20};
    RECT value_rect = {rect.left + 10, rect.top + 21, rect.right - 70, rect.bottom - 4};
    RECT state_rect = {rect.right - 66, rect.top + 21, rect.right - 8, rect.bottom - 4};
    fill_rect(hdc, rect, RGB(31, 38, 40));
    fill_rect(hdc, stripe, accent);
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_END_ELLIPSIS);
    draw_text_rect(hdc, value_rect, value, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_END_ELLIPSIS);
    draw_text_rect(hdc, state_rect, state, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_RIGHT | DT_END_ELLIPSIS);
}

static void draw_bonus_grid(HDC hdc, UiCursor *cursor, int stage,
                            TechnologyBonusSummary now, TechnologyBonusSummary next) {
    int w = (cursor->width - 8) / 2;
    int i;
    for (i = 0; i < 7; i++) {
        int col = i % 2;
        RECT r = {cursor->x + col * (w + 8), cursor->y,
                  cursor->x + col * (w + 8) + w, cursor->y + 42};
        char value[32];
        const char *state;
        COLORREF accent = bonus_locked(i, stage, now) ? RGB(82, 96, 100) : bonus_function_color(i);
        if (bonus_is_unlock_type(i)) state = "";
        else if (stage == 0 || bonus_is_base(i, now)) state = tr("Base", "基础");
        else state = tr("Active", "生效");
        if (!bonus_locked(i, stage, now) && bonus_changed(i, stage, now, next)) {
            accent = blend_color(bonus_function_color(i), RGB(232, 196, 88), 38);
        }
        bonus_text(i, stage, now, value, sizeof(value));
        draw_bonus_cell(hdc, r, bonus_label(i, stage), value, state, accent);
        if (col == 1 || i == 6) cursor->y += 50;
    }
}

static void draw_factor_mini_bar(HDC hdc, UiCursor *cursor, const char *label,
                                 int value, int max_value, const char *suffix, COLORREF color) {
    RECT row = ui_take_rect(cursor, 24);
    RECT name = {row.left, row.top, row.left + 76, row.bottom};
    RECT value_rect = {row.left + 78, row.top, row.left + 126, row.bottom};
    RECT bar = {row.left + 134, row.top + 7, row.right, row.bottom - 7};
    char text[48];
    draw_text_rect(hdc, name, label, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%d%s", value, suffix);
    draw_text_rect(hdc, value_rect, text,
                   ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    ui_progress_bar(hdc, bar, value, max_value, color);
}

static void draw_progress_balance_bar(HDC hdc, UiCursor *cursor, int acceleration, int drag) {
    RECT row = ui_take_rect(cursor, 30);
    RECT left_label = {row.left, row.top, row.left + 44, row.bottom};
    RECT right_label = {row.right - 44, row.top, row.right, row.bottom};
    RECT bar = {row.left + 50, row.top + 9, row.right - 50, row.bottom - 9};
    int mid = (bar.left + bar.right) / 2;
    RECT left_fill = {mid - (mid - bar.left) * clamp(acceleration, 0, 100) / 100, bar.top, mid, bar.bottom};
    RECT right_fill = {mid, bar.top, mid + (bar.right - mid) * clamp(drag, 0, 100) / 100, bar.bottom};

    draw_text_rect(hdc, left_label, tr("Accel", "加速"), ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, right_label, tr("Drag", "阻碍"), ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_END_ELLIPSIS);
    fill_rect(hdc, bar, RGB(30, 35, 38));
    fill_rect(hdc, left_fill, RGB(72, 160, 146));
    fill_rect(hdc, right_fill, RGB(190, 92, 68));
    fill_rect(hdc, (RECT){mid - 1, bar.top - 3, mid + 1, bar.bottom + 3}, RGB(166, 174, 176));
}

static void draw_expected_advance(HDC hdc, UiCursor *cursor, int stage, int months) {
    char text[96];
    if (stage >= 10) {
        ui_row_text(hdc, cursor, tr("Expected Advance", "预计进阶"), tr("Maximum stage reached", "已达到最高阶段"));
        return;
    }
    ui_format_months(text, sizeof(text), max(0, months), UI_MONTH_ZERO_NOW);
    ui_row_text(hdc, cursor, tr("Expected Advance", "预计进阶"), text);
}

static void draw_stage_detail(HDC hdc, UiCursor *cursor, int civ_id, int inspect_stage) {
    int current = clamp(civs[civ_id].tech_stage, 0, 10);
    const char *status = inspect_stage < current ? tr("Completed", "已完成") :
                         (inspect_stage == current ? tr("Current", "当前") : tr("Locked", "未解锁"));
    char text[96];
    ui_section(hdc, cursor, tr("Stage Detail", "阶段详情"));
    snprintf(text, sizeof(text), "%s %d: %s", tr("Stage", "阶段"), inspect_stage,
             technology_stage_name(inspect_stage, ui_language));
    ui_row_text(hdc, cursor, status, text);
    ui_row_text(hdc, cursor, tr("Unlock", "解锁"), technology_stage_effect(inspect_stage, ui_language));
    if (inspect_stage == current) {
        snprintf(text, sizeof(text), "%d%%", technology_stage_progress_percent(civ_id));
        ui_row_text(hdc, cursor, tr("Progress", "进度"), text);
    } else if (inspect_stage > current) {
        int months = technology_months_to_next(civ_id) +
                     (inspect_stage - current - 1) * technology_required_months_for_civ(civ_id);
        ui_format_months(text, sizeof(text), months, UI_MONTH_ZERO_NOW);
        ui_row_text(hdc, cursor, tr("Estimated arrival", "预计到达"), text);
    }
}

int country_tech_tab_height(int civ_id) {
    (void)civ_id;
    return 620;
}

void draw_country_tech_tab(HDC hdc, UiCursor *cursor, int civ_id) {
    TechnologyBonusSummary current, next;
    CountrySummary country = summarize_country(civ_id);
    int stage = clamp(civs[civ_id].tech_stage, 0, 10);
    int progress = technology_stage_progress_percent(civ_id);
    int months = technology_months_to_next(civ_id);
    RECT track = {cursor->x + 12, cursor->y + 26, cursor->x + cursor->width - 12, cursor->y + 26};
    char text[160];
    char span[80];

    technology_current_bonus_summary(civ_id, &current);
    technology_next_bonus_summary(civ_id, &next);
    ui_section(hdc, cursor, tr("Technology Stage", "科技阶段"));
    draw_stage_track(hdc, track, stage, progress);
    cursor->y += 48;
    snprintf(text, sizeof(text), "%s %d: %s", tr("Stage", "阶段"), stage,
             technology_stage_name(stage, ui_language));
    ui_row_text(hdc, cursor, tr("Current", "当前"), text);
    if (stage == 0) {
        ui_row_text(hdc, cursor, tr("Stage State", "阶段状态"),
                    tr("No technology bonus yet.", "暂无科技加成。"));
    }
    ui_progress_bar(hdc, ui_take_rect(cursor, 12), progress, 100, RGB(104, 158, 186));
    ui_format_months(span, sizeof(span), months, UI_MONTH_ZERO_DONE);
    snprintf(text, sizeof(text), "%d%%   %s", progress, span);
    ui_row_text(hdc, cursor, tr("To Next", "距下阶段"), stage >= 10 ? tr("Complete", "完成") : text);

    ui_section(hdc, cursor, tr("Bonus Grid", "加成网格"));
    draw_bonus_grid(hdc, cursor, stage, current, next);
    if (stage < 10) {
        snprintf(text, sizeof(text), "%s %d: %s", tr("Next", "下阶段"), stage + 1,
                 technology_stage_name(stage + 1, ui_language));
        ui_row_text(hdc, cursor, tr("Preview", "预览"), text);
    }

    {
        int tech_force = clamp(civs[civ_id].innovation * 10, 0, 100);
        int resource_force = clamp(country.resource_score * 2, 0, 100);
        int pressure_drag = clamp(population_pressure_for_civ(civ_id), 0, 100);
        int disorder_drag = clamp(100 - disorder_technology_percent(civs[civ_id].disorder), 0, 100);
        int acceleration = clamp((tech_force + resource_force) / 2, 0, 100);
        int drag = clamp((pressure_drag + disorder_drag) / 2, 0, 100);

        ui_section(hdc, cursor, tr("Progress Balance", "推进平衡"));
        draw_factor_mini_bar(hdc, cursor, tr("Tech", "技术"), civs[civ_id].innovation, 10, "", RGB(104, 158, 186));
        draw_factor_mini_bar(hdc, cursor, tr("Resources", "资源"), country.resource_score, 100, "", RGB(190, 156, 78));
        draw_factor_mini_bar(hdc, cursor, tr("Population", "人口"), pressure_drag, 100, "%", RGB(178, 128, 74));
        draw_factor_mini_bar(hdc, cursor, tr("Disorder", "混乱"), disorder_drag, 100, "%", RGB(178, 84, 74));
        draw_progress_balance_bar(hdc, cursor, acceleration, drag);
        draw_expected_advance(hdc, cursor, stage, months);
    }

    draw_stage_detail(hdc, cursor, civ_id, hovered_stage_node(track, stage));
}
