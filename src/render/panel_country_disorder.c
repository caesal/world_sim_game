#include "render/panel_country_disorder.h"

#include "render/snapshot_ui.h"
#include "render/ui_format.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    const char *label;
    IconId icon;
    int value;
    int contribution;
    int decay;
    int eta_months;
    const char *status;
    COLORREF color;
    int structural;
} PressureCard;

static const SnapshotCiv zero_civ;

static const SnapshotCiv *disorder_snapshot_civ(int civ_id) {
    const SnapshotCiv *civ = snapshot_ui_civ(civ_id);
    return civ ? civ : &zero_civ;
}

#define CIV(civ_id) (disorder_snapshot_civ(civ_id))

static int disorder_ui_effect_percent(int disorder) {
    disorder = clamp(disorder, 0, 100);
    if (disorder <= 25) return 100;
    if (disorder >= 74) return 80;
    return 95 - (disorder - 26) * 15 / 48;
}

static int disorder_ui_pressure_eta_months(int value, int monthly_decay) {
    if (value <= 0) return 0;
    if (monthly_decay <= 0) return -1;
    return (value + monthly_decay - 1) / monthly_decay;
}

static int collapse_ui_chance_for_disorder(int disorder) {
    if (disorder >= 100) return 0;
    if (disorder >= 95) return 65;
    if (disorder >= 90) return 45;
    if (disorder >= 85) return 30;
    if (disorder >= 80) return 20;
    if (disorder >= 75) return 10;
    return 0;
}

int country_disorder_tab_height(int civ_id) {
    (void)civ_id;
    return 840;
}

static COLORREF net_color(int net) {
    if (net < 0) return RGB(118, 188, 112);
    if (net > 0) return RGB(218, 92, 78);
    return ui_theme_color(UI_COLOR_TEXT_MUTED);
}

static void format_disorder_months(char *buffer, size_t size, int months) {
    if (months == -3) snprintf(buffer, size, "%s", tr("hard floor", "硬下限"));
    else if (months == -2) snprintf(buffer, size, "%s", tr("after plague ends", "瘟疫结束后计算"));
    else if (months < 0) snprintf(buffer, size, "%s", tr("condition based", "条件决定"));
    else ui_format_months(buffer, size, months, UI_MONTH_ZERO_DONE);
}

static void format_x10(char *buffer, size_t size, int value_x10) {
    snprintf(buffer, size, "%d.%d", value_x10 / 10, abs(value_x10 % 10));
}

static void format_signed_x10(char *buffer, size_t size, int value_x10) {
    int abs_value = value_x10 < 0 ? -value_x10 : value_x10;
    snprintf(buffer, size, "%c%d.%d", value_x10 < 0 ? '-' : '+', abs_value / 10, abs_value % 10);
}

static void format_net_x10(char *buffer, size_t size, int net_x10) {
    char value[32];
    const char *arrow = net_x10 < 0 ? "v" : (net_x10 > 0 ? "^" : ">");
    format_signed_x10(value, sizeof(value), net_x10);
    snprintf(buffer, size, "%s %s/%s", arrow, value, tr("mo", "月"));
}

static void draw_risk_bar(HDC hdc, RECT rect, int disorder) {
    RECT seg;
    int x25 = rect.left + (rect.right - rect.left) * 25 / 100;
    int x75 = rect.left + (rect.right - rect.left) * 75 / 100;
    int x95 = rect.left + (rect.right - rect.left) * 95 / 100;
    int px = rect.left + (rect.right - rect.left) * clamp(disorder, 0, 100) / 100;
    fill_rect(hdc, rect, RGB(30, 35, 38));
    seg = (RECT){rect.left, rect.top, x25, rect.bottom};
    fill_rect(hdc, seg, RGB(93, 154, 93));
    seg = (RECT){x25, rect.top, x75, rect.bottom};
    fill_rect(hdc, seg, RGB(204, 154, 68));
    seg = (RECT){x75, rect.top, x95, rect.bottom};
    fill_rect(hdc, seg, RGB(194, 82, 68));
    seg = (RECT){x95, rect.top, rect.right, rect.bottom};
    fill_rect(hdc, seg, RGB(112, 36, 42));
    fill_rect(hdc, (RECT){px - 1, rect.top - 4, px + 2, rect.bottom + 4}, RGB(248, 236, 188));
}

static void draw_overview(HDC hdc, UiCursor *cursor, int civ_id) {
    char text[256];
    char a[32], b[32], c[32], d[32], e[32];
    RECT card = ui_take_rect(cursor, 144);
    RECT bar = {card.left + 10, card.top + 48, card.right - 10, card.top + 64};
    RECT recovery_bar = {card.left + 10, card.top + 104, card.right - 10, card.top + 114};
    int net_x10 = CIV(civ_id)->disorder_last_net_x10;
    int actual_net_x10 = CIV(civ_id)->disorder <= 0 && net_x10 < 0 ? 0 : net_x10;
    int recovery_x10 = max(1, CIV(civ_id)->disorder_last_recovery_x10);
    int x = recovery_bar.left;
    int width = recovery_bar.right - recovery_bar.left;
    fill_rect(hdc, card, ui_theme_color(UI_COLOR_PANEL_SOFT));
    snprintf(text, sizeof(text), "%s %d / 100", tr("Total disorder", "总混乱"), CIV(civ_id)->disorder);
    draw_text_rect(hdc, (RECT){card.left + 10, card.top + 8, card.right - 110, card.top + 32},
                   text, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    format_net_x10(text, sizeof(text), actual_net_x10);
    draw_text_rect(hdc, (RECT){card.right - 100, card.top + 8, card.right - 10, card.top + 32},
                   text, net_color(actual_net_x10), DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_END_ELLIPSIS);
    draw_risk_bar(hdc, bar, CIV(civ_id)->disorder);
    snprintf(text, sizeof(text), "%s +%d/%s   %s -%d/%s",
             tr("Pressure", "压力"), CIV(civ_id)->disorder_last_pressure, tr("mo", "月"),
             tr("Recovery", "恢复"), CIV(civ_id)->disorder_last_recovery, tr("mo", "月"));
    draw_text_rect(hdc, (RECT){card.left + 10, card.top + 68, card.right - 10, card.bottom - 6},
                   text, ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    fill_rect(hdc, (RECT){card.left + 8, card.top + 66, card.right - 8, card.bottom - 6},
              ui_theme_color(UI_COLOR_PANEL_SOFT));
    format_x10(a, sizeof(a), CIV(civ_id)->disorder_last_pressure_x10);
    format_x10(b, sizeof(b), CIV(civ_id)->disorder_last_recovery_x10);
    format_signed_x10(c, sizeof(c), actual_net_x10);
    snprintf(text, sizeof(text), "%s +%s/%s   %s %s/%s   %s %s/%s",
             tr("Pressure", "压力"), a, tr("mo", "月"),
             tr("Recovery ability", "恢复能力"), b, tr("mo", "月"),
             tr("Actual change", "实际变化"), c, tr("mo", "月"));
    draw_text_rect(hdc, (RECT){card.left + 10, card.top + 68, card.right - 10, card.top + 86},
                   text, ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (CIV(civ_id)->disorder <= 0 && net_x10 < 0) {
        draw_text_rect(hdc, (RECT){card.left + 10, card.top + 86, card.right - 10, card.top + 102},
                       tr("At 0, recovery cannot reduce disorder further.", "当前混乱已为 0，恢复不会继续降低总混乱。"),
                       ui_theme_color(UI_COLOR_TEXT_DIM), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
    fill_rect(hdc, recovery_bar, RGB(31, 36, 39));
#define REC_SEG(value, color) do { int seg = width * (value) / recovery_x10; fill_rect(hdc, (RECT){x, recovery_bar.top, x + seg, recovery_bar.bottom}, (color)); x += seg; } while (0)
    REC_SEG(CIV(civ_id)->disorder_last_base_recovery_x10, RGB(105, 150, 120));
    REC_SEG(CIV(civ_id)->disorder_last_governance_recovery_x10, RGB(86, 142, 184));
    REC_SEG(CIV(civ_id)->disorder_last_peace_recovery_x10, RGB(170, 154, 88));
    REC_SEG(CIV(civ_id)->disorder_last_cohesion_recovery_x10, RGB(142, 118, 186));
    REC_SEG(CIV(civ_id)->disorder_last_condition_recovery_x10, RGB(118, 166, 106));
#undef REC_SEG
    format_x10(a, sizeof(a), CIV(civ_id)->disorder_last_base_recovery_x10);
    format_x10(b, sizeof(b), CIV(civ_id)->disorder_last_governance_recovery_x10);
    format_x10(c, sizeof(c), CIV(civ_id)->disorder_last_peace_recovery_x10);
    format_x10(d, sizeof(d), CIV(civ_id)->disorder_last_cohesion_recovery_x10);
    format_x10(e, sizeof(e), CIV(civ_id)->disorder_last_condition_recovery_x10);
    snprintf(text, sizeof(text), "%s %s  %s %s  %s %s  %s %s  %s %s",
             tr("Base", "基础"), a, tr("Gov", "治理"), b, tr("Peace", "和平"), c,
             tr("Cohesion", "凝聚"), d, tr("Cond", "条件"), e);
    draw_text_rect(hdc, (RECT){card.left + 10, card.top + 118, card.right - 10, card.bottom - 6},
                   text, ui_theme_color(UI_COLOR_TEXT_DIM), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    cursor->y += 8;
}

static void draw_pressure_card(HDC hdc, RECT card, const PressureCard *src) {
    char text[128];
    char eta[48];
    RECT stripe = card;
    RECT bar = {card.left + 12, card.top + 44, card.right - 12, card.top + 54};
    fill_rect(hdc, card, ui_theme_color(UI_COLOR_PANEL_SOFT));
    stripe.right = stripe.left + 4;
    fill_rect(hdc, stripe, src->color);
    draw_icon(hdc, src->icon, (RECT){card.left + 12, card.top + 8, card.left + 30, card.top + 26}, src->color);
    draw_text_rect(hdc, (RECT){card.left + 36, card.top + 6, card.right - 12, card.top + 25},
                   src->label, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%d / 100", clamp(src->value, 0, 100));
    draw_text_rect(hdc, (RECT){card.left + 36, card.top + 24, card.right - 12, card.top + 42},
                   text, ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    ui_progress_bar(hdc, bar, src->value, 100, src->color);
    format_disorder_months(eta, sizeof(eta), src->eta_months);
    if (src->structural) snprintf(text, sizeof(text), "%s", tr("Hard floor, no decay", "硬下限，不自然衰退"));
    else snprintf(text, sizeof(text), "+%d/%s   -%d/%s", src->contribution, tr("mo", "月"), src->decay, tr("mo", "月"));
    if (!src->structural) {
        char contribution[32];
        format_x10(contribution, sizeof(contribution), src->contribution);
        snprintf(text, sizeof(text), "+%s/%s   -%d/%s", contribution, tr("mo", "月"), src->decay, tr("mo", "月"));
    }
    draw_text_rect(hdc, (RECT){card.left + 12, card.top + 58, card.right - 12, card.top + 78},
                   text, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%s: %s", tr("Clear", "清零"), eta);
    draw_text_rect(hdc, (RECT){card.left + 12, card.top + 78, card.right - 12, card.top + 98},
                   text, ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, (RECT){card.left + 12, card.top + 98, card.right - 12, card.bottom - 5},
                   src->status, src->color, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_pressure_cards(HDC hdc, UiCursor *cursor, int civ_id) {
    int plague_active = (CIV(civ_id)->plague_active_count > 0);
    int war_active = CIV(civ_id)->war_active;
    int plague_decay = CIV(civ_id)->disorder_last_plague_decay;
    int war_decay = CIV(civ_id)->disorder_last_war_decay;
    int migration_decay = CIV(civ_id)->disorder_last_migration_decay;
    PressureCard cards[5];
    int cols = cursor->width >= 340 ? 2 : 1;
    int gap = 8;
    int h = 124;
    int w = (cursor->width - (cols - 1) * gap) / cols;
    int i;
    ui_section(hdc, cursor, tr("Pressure Sources", "压力来源"));
    cards[0] = (PressureCard){tr("Resource pressure", "资源压力"), ICON_FOOD, CIV(civ_id)->disorder_resource,
                              CIV(civ_id)->disorder_resource * 10 / 18, 0, -1,
                              tr("Current conditions", "当前条件决定"), RGB(204, 154, 68), 0};
    cards[1] = (PressureCard){tr("Plague pressure", "瘟疫压力"), ICON_DISORDER, CIV(civ_id)->disorder_plague,
                              CIV(civ_id)->disorder_plague * 10 / 24, plague_decay,
                              plague_active ? -2 : disorder_ui_pressure_eta_months(CIV(civ_id)->disorder_plague, plague_decay),
                              plague_active ? tr("Plague active", "瘟疫活跃") : tr("Post-plague recovery", "瘟疫后恢复"),
                              RGB(44, 116, 82), 0};
    cards[2] = (PressureCard){tr("War / unrest", "战争/内乱"), ICON_BATTLE, CIV(civ_id)->disorder_stability,
                              CIV(civ_id)->disorder_stability * 10 / 28, war_decay,
                              disorder_ui_pressure_eta_months(CIV(civ_id)->disorder_stability, war_decay),
                              war_active ? tr("At war", "战争中") : tr("Post-war recovery", "战后恢复"),
                              RGB(188, 74, 66), 0};
    cards[3] = (PressureCard){tr("Migration / expansion", "迁徙/扩张"), ICON_MIGRATION, CIV(civ_id)->disorder_migration,
                              CIV(civ_id)->disorder_migration * 10 / 26, migration_decay,
                              disorder_ui_pressure_eta_months(CIV(civ_id)->disorder_migration, migration_decay),
                              tr("Temporary friction", "临时摩擦"), RGB(72, 146, 176), 0};
    cards[4] = (PressureCard){tr("Vassal governance", "附庸治理"), ICON_GOVERNANCE,
                              CIV(civ_id)->vassal_governance_disorder, 0, 0, -3,
                              tr("Hard floor", "硬下限"), RGB(142, 105, 178), 1};
    for (i = 0; i < 5; i++) {
        int col = i % cols;
        int row = i / cols;
        RECT card = {cursor->x + col * (w + gap), cursor->y + row * (h + gap),
                     cursor->x + col * (w + gap) + w, cursor->y + row * (h + gap) + h};
        draw_pressure_card(hdc, card, &cards[i]);
    }
    cursor->y += ((5 + cols - 1) / cols) * (h + gap) + 4;
}

static void draw_plague_status(HDC hdc, UiCursor *cursor, int civ_id) {
    char text[160];
    char span[48];
    int active = CIV(civ_id)->plague_active_count;
    int immunity = CIV(civ_id)->plague_random_immunity_months;
    RECT card;

    ui_section(hdc, cursor, tr("Plague Status", "瘟疫状态"));
    card = ui_take_rect(cursor, active > 0 ? 112 : 82);
    fill_rect(hdc, card, ui_theme_color(UI_COLOR_PANEL_SOFT));
    if (immunity > 0) {
        ui_format_months(span, sizeof(span), immunity, UI_MONTH_ZERO_DONE);
        snprintf(text, sizeof(text), "%s: %s", tr("Random plague immunity", "瘟疫随机免疫"), span);
        draw_text_rect(hdc, (RECT){card.left + 10, card.top + 8, card.right - 10, card.top + 30},
                       text, RGB(128, 184, 146), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        draw_text_rect(hdc, (RECT){card.left + 10, card.top + 32, card.right - 10, card.top + 54},
                       tr("Cannot start naturally; can still be infected externally.",
                          "不会自然爆发，可被外部传染。"),
                       ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    } else {
        snprintf(text, sizeof(text), "%s: %s", tr("Random plague immunity", "瘟疫随机免疫"), tr("None", "无"));
        draw_text_rect(hdc, (RECT){card.left + 10, card.top + 8, card.right - 10, card.top + 30},
                       text, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        draw_text_rect(hdc, (RECT){card.left + 10, card.top + 32, card.right - 10, card.top + 54},
                       tr("Can start naturally.", "可自然爆发。"),
                       ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
    if (active > 0) {
        ui_format_months(span, sizeof(span), CIV(civ_id)->plague_months_left, UI_MONTH_ZERO_DONE);
        snprintf(text, sizeof(text), "%s: %s   %s %d   %s %d",
                 tr("Current plague", "当前瘟疫"), tr("Active", "活跃"),
                 tr("Cities", "感染城市"), active, tr("Peak", "最高烈度"), CIV(civ_id)->plague_peak_severity);
        draw_text_rect(hdc, (RECT){card.left + 10, card.top + 60, card.right - 10, card.top + 82},
                       text, RGB(190, 220, 196), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        snprintf(text, sizeof(text), "%s: %s", tr("Remaining", "剩余"), span);
        draw_text_rect(hdc, (RECT){card.left + 10, card.top + 84, card.right - 10, card.bottom - 8},
                       text, ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
    cursor->y += 8;
}

static void draw_effect_bar(HDC hdc, UiCursor *cursor, const char *label, int final_percent,
                            int tech_percent, int disorder_percent, COLORREF color) {
    char text[128];
    RECT row = ui_take_rect(cursor, 38);
    RECT label_rect = {row.left, row.top, row.left + 92, row.top + 20};
    RECT value_rect = {row.left + 96, row.top, row.right, row.top + 20};
    RECT bar = {row.left, row.top + 23, row.right, row.bottom - 5};
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%d%%   %s %d%%   %s -%d%%",
             final_percent, tr("Tech", "科技"), tech_percent,
             tr("Disorder impact", "混乱影响"), 100 - disorder_percent);
    draw_text_rect(hdc, value_rect, text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_END_ELLIPSIS);
    ui_progress_bar(hdc, bar, final_percent, 140, color);
}

static void draw_effects(HDC hdc, UiCursor *cursor, int civ_id) {
    int disorder = CIV(civ_id)->disorder;
    int resource_disorder = disorder_ui_effect_percent(disorder);
    int tech_disorder = disorder_ui_effect_percent(disorder);
    int population_disorder = disorder_ui_effect_percent(disorder);
    int tech_resource = CIV(civ_id)->tech_resource_percent;
    int tech_progress = CIV(civ_id)->tech_progress_percent;
    ui_section(hdc, cursor, tr("Active Effects", "当前影响"));
    draw_effect_bar(hdc, cursor, tr("Resource output", "资源产出"),
                    tech_resource * resource_disorder / 100, tech_resource, resource_disorder, RGB(204, 154, 68));
    draw_effect_bar(hdc, cursor, tr("Population growth", "人口增长"),
                    population_disorder, 100, population_disorder, RGB(84, 164, 184));
    draw_effect_bar(hdc, cursor, tr("Tech progress", "科技推进"),
                    tech_progress * tech_disorder / 100, tech_progress, tech_disorder, RGB(104, 150, 205));
}

static const char *collapse_risk_text(int civ_id) {
    static char text[96];
    int disorder = CIV(civ_id)->disorder;
    int grace = CIV(civ_id)->collapse_grace_months;
    int chance = collapse_ui_chance_for_disorder(disorder);
    if (disorder >= 100) snprintf(text, sizeof(text), "%s", tr("Immediate collapse attempt", "立即尝试崩溃"));
    else if (grace > 0) snprintf(text, sizeof(text), "%s", tr("Collapse grace active", "崩溃保护期"));
    else if (chance <= 0) snprintf(text, sizeof(text), "%s", tr("No scheduled collapse risk", "无定期崩溃风险"));
    else snprintf(text, sizeof(text), "%d%% / %s", chance, tr("25 years", "每 25 年"));
    return text;
}

static void draw_collapse_risk(HDC hdc, UiCursor *cursor, int civ_id) {
    char text[192];
    char span[48];
    int disorder = CIV(civ_id)->disorder;
    int grace = CIV(civ_id)->collapse_grace_months;
    int chance = collapse_ui_chance_for_disorder(disorder);
    COLORREF accent = disorder >= 100 ? RGB(136, 34, 44) :
                      chance > 0 ? RGB(196, 82, 68) : RGB(96, 150, 106);
    RECT card;
    ui_section(hdc, cursor, tr("Collapse Risk", "崩溃风险"));
    card = ui_take_rect(cursor, 126);
    fill_rect(hdc, card, ui_theme_color(UI_COLOR_PANEL_SOFT));
    fill_rect(hdc, (RECT){card.left, card.top, card.left + 4, card.bottom}, accent);
    draw_text_rect(hdc, (RECT){card.left + 12, card.top + 8, card.right - 12, card.top + 30},
                   collapse_risk_text(civ_id), accent, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (disorder < 75) snprintf(text, sizeof(text), "%s", tr("Current disorder is below 75.", "当前混乱低于 75。"));
    else if (grace > 0) snprintf(text, sizeof(text), "%s", tr("Ordinary 25-year roll is skipped.", "普通 25 年判定已跳过。"));
    else if (disorder >= 100) snprintf(text, sizeof(text), "%s", tr("Immediate path owns this check.", "由立即崩溃路径处理。"));
    else snprintf(text, sizeof(text), "%s", tr("Ordinary check waiting.", "等待普通周期判定。"));
    draw_text_rect(hdc, (RECT){card.left + 12, card.top + 32, card.right - 12, card.top + 54},
                   text, ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (grace > 0) format_disorder_months(span, sizeof(span), grace);
    else if (chance > 0) {
        const RenderSnapshot *snapshot = snapshot_ui_current();
        int y = snapshot ? snapshot->year : 0;
        int m = snapshot ? snapshot->month : 1;
        format_disorder_months(span, sizeof(span), max(1, (25 - (y % 25)) * 12 - m + 1));
    }
    else snprintf(span, sizeof(span), "%s", tr("none", "无"));
    snprintf(text, sizeof(text), "%s: %s", tr("Next check", "下次判定"), span);
    draw_text_rect(hdc, (RECT){card.left + 12, card.top + 56, card.right - 12, card.top + 78},
                   text, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%s: %s", tr("Last result", "最近判定"), CIV(civ_id)->collapse_last_reason);
    draw_text_rect(hdc, (RECT){card.left + 12, card.top + 80, card.right - 12, card.bottom - 8},
                   text, ui_theme_color(UI_COLOR_TEXT_MUTED), DT_WORDBREAK | DT_END_ELLIPSIS);
    cursor->y += 8;
}

void draw_country_disorder_tab(HDC hdc, UiCursor *cursor, int civ_id) {
    ui_section(hdc, cursor, tr("Disorder Overview", "混乱总览"));
    draw_overview(hdc, cursor, civ_id);
    draw_pressure_cards(hdc, cursor, civ_id);
    draw_plague_status(hdc, cursor, civ_id);
    draw_effects(hdc, cursor, civ_id);
    draw_collapse_risk(hdc, cursor, civ_id);
}
