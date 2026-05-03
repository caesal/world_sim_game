#include "render_internal.h"

static int displayed_civ_id(void) {
    int owner = selected_tile_owner();

    if (owner >= 0 && owner < civ_count) return owner;
    if (selected_civ >= 0 && selected_civ < civ_count) return selected_civ;
    return -1;
}

static const char *diplomacy_status_label(DiplomacyStatus status) {
    switch (status) {
        case DIPLOMACY_PEACE: return tr("Peace", "和平");
        case DIPLOMACY_ALLIANCE: return tr("Alliance", "联盟");
        case DIPLOMACY_TENSE: return tr("Tense", "紧张");
        case DIPLOMACY_TRUCE: return tr("Truce", "停战");
        case DIPLOMACY_WAR: return tr("War", "战争");
        case DIPLOMACY_VASSAL: return tr("Vassal", "附庸");
        case DIPLOMACY_NONE: return tr("Unknown", "未知");
        default: return tr("Unknown", "未知");
    }
}

static COLORREF diplomacy_status_color(DiplomacyStatus status) {
    switch (status) {
        case DIPLOMACY_PEACE: return RGB(86, 150, 96);
        case DIPLOMACY_ALLIANCE: return RGB(88, 134, 190);
        case DIPLOMACY_TENSE: return RGB(196, 154, 72);
        case DIPLOMACY_TRUCE: return RGB(128, 148, 170);
        case DIPLOMACY_WAR: return RGB(176, 72, 62);
        case DIPLOMACY_VASSAL: return RGB(154, 105, 178);
        default: return RGB(104, 112, 120);
    }
}

static void draw_status_badge(HDC hdc, RECT rect, DiplomacyStatus status) {
    fill_rect(hdc, rect, diplomacy_status_color(status));
    draw_center_text(hdc, rect, diplomacy_status_label(status), RGB(246, 248, 250));
}

static long long province_garrison_weight(int city_id) {
    RegionSummary summary;
    long long weight;

    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) return 0;
    summary = summarize_city_region(city_id);
    weight = (long long)summary.population + summary.tiles * 8LL + summary.defense * 80LL;
    if (cities[city_id].capital) weight += 500;
    if (cities[city_id].port) weight += 160;
    return weight > 0 ? weight : 1;
}

static long long total_garrison_weight(int civ_id) {
    long long total = 0;
    int i;

    for (i = 0; i < city_count; i++) {
        if (cities[i].alive && cities[i].owner == civ_id) total += province_garrison_weight(i);
    }
    return total > 0 ? total : 1;
}

static int estimate_city_garrison(int city_id, int total_soldiers, long long total_weight) {
    long long weight = province_garrison_weight(city_id);
    long long estimate;

    if (total_soldiers <= 0 || weight <= 0 || total_weight <= 0) return 0;
    estimate = (long long)total_soldiers * weight / total_weight;
    if (estimate <= 0) estimate = 1;
    return (int)clamp((int)estimate, 0, MAX_POPULATION);
}

static void draw_war_progress_bar(HDC hdc, RECT bar, int selected, int opponent, ActiveWar war) {
    int own_initial = selected == war.attacker ? war.initial_soldiers_a : war.initial_soldiers_b;
    int enemy_initial = selected == war.attacker ? war.initial_soldiers_b : war.initial_soldiers_a;
    int own_soldiers = selected == war.attacker ? war.soldiers_a : war.soldiers_b;
    int enemy_soldiers = selected == war.attacker ? war.soldiers_b : war.soldiers_a;
    int own_losses = selected == war.attacker ? war.casualties_a : war.casualties_b;
    int enemy_losses = selected == war.attacker ? war.casualties_b : war.casualties_a;
    int own_wins = selected == war.attacker ? war.wins_a : war.wins_b;
    int enemy_wins = selected == war.attacker ? war.wins_b : war.wins_a;
    int own_loss_pct = own_initial > 0 ? own_losses * 100 / own_initial : 0;
    int enemy_loss_pct = enemy_initial > 0 ? enemy_losses * 100 / enemy_initial : 0;
    int left_pct = clamp(50 + (enemy_loss_pct - own_loss_pct) * 45 / 100, 8, 92);
    RECT left = bar;
    RECT center = bar;
    char text[256];
    char own_text[32];
    char enemy_text[32];
    char own_loss_text[32];
    char enemy_loss_text[32];

    left.right = bar.left + (bar.right - bar.left) * left_pct / 100;
    fill_rect(hdc, bar, civs[opponent].color);
    fill_rect(hdc, left, civs[selected].color);
    center.left = left.right - 1;
    center.right = left.right + 1;
    fill_rect(hdc, center, RGB(232, 238, 244));
    format_metric_value(own_soldiers, own_text, sizeof(own_text));
    format_metric_value(enemy_soldiers, enemy_text, sizeof(enemy_text));
    format_metric_value(own_losses, own_loss_text, sizeof(own_loss_text));
    format_metric_value(enemy_losses, enemy_loss_text, sizeof(enemy_loss_text));
    snprintf(text, sizeof(text), "%s %s / %s   %s %s / %s   %s %d-%d",
             tr("Soldiers", "兵力"), own_text, enemy_text,
             tr("Losses", "损失"), own_loss_text, enemy_loss_text,
             tr("Wins", "胜场"), own_wins, enemy_wins);
    draw_center_text(hdc, bar, text, RGB(252, 252, 250));
}

static int draw_military_section(HDC hdc, RECT client, int x, int y, int civ_id, HFONT title_font, HFONT body_font) {
    char text[256];
    int total_soldiers = war_current_soldiers_for_civ(civ_id);
    int reserve_soldiers = war_estimated_soldiers(civ_id);
    long long total_weight = total_garrison_weight(civ_id);
    int capital_id = civs[civ_id].capital_city;
    int shown = 0;
    int hidden = 0;
    int i;

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, y, tr("Military", "军事"), RGB(245, 245, 245));
    y += 28;
    SelectObject(hdc, body_font);
    {
        char current_text[32];
        char reserve_text[32];

        format_metric_value(total_soldiers, current_text, sizeof(current_text));
        format_metric_value(reserve_soldiers, reserve_text, sizeof(reserve_text));
        snprintf(text, sizeof(text), "%s: %s   %s: %s",
                 tr("Available soldiers", "可用兵力"), current_text,
                 tr("Mobilization base", "动员基数"), reserve_text);
        draw_icon_text_line(hdc, x, y, ICON_BATTLE, text, RGB(220, 226, 232));
        y += 24;
    }
    if (capital_id >= 0 && capital_id < city_count && cities[capital_id].alive) {
        char garrison_text[32];
        int garrison = estimate_city_garrison(capital_id, total_soldiers, total_weight);

        format_metric_value(garrison, garrison_text, sizeof(garrison_text));
        snprintf(text, sizeof(text), "%s: %s   %s %s",
                 tr("Capital", "首都"), cities[capital_id].name,
                 tr("Garrison", "驻军"), garrison_text);
        draw_icon_text_line(hdc, x, y, ICON_CITY_CAPITAL, text, RGB(220, 226, 232));
        y += 26;
    }
    draw_text_line(hdc, x, y, tr("Province Distribution", "行省分布"), RGB(205, 214, 222));
    y += 22;
    for (i = 0; i < city_count; i++) {
        int garrison;
        char garrison_text[32];
        char pop_text[32];

        if (!cities[i].alive || cities[i].owner != civ_id) continue;
        if (y > client.bottom - 42) {
            hidden++;
            continue;
        }
        garrison = estimate_city_garrison(i, total_soldiers, total_weight);
        format_metric_value(garrison, garrison_text, sizeof(garrison_text));
        format_metric_value(cities[i].population, pop_text, sizeof(pop_text));
        snprintf(text, sizeof(text), "%s%s  %s %s  %s %s",
                 cities[i].capital ? "* " : "",
                 cities[i].name,
                 tr("Garrison", "驻军"), garrison_text,
                 tr("Pop", "人口"), pop_text);
        draw_icon_text_line(hdc, x + 8, y, cities[i].capital ? ICON_CITY_CAPITAL : ICON_TERRITORY,
                            text, RGB(200, 211, 220));
        y += 24;
        shown++;
    }
    if (shown == 0) {
        draw_text_line(hdc, x + 8, y, tr("No provinces yet.", "还没有行省。"), RGB(160, 171, 180));
        y += 22;
    } else if (hidden > 0) {
        snprintf(text, sizeof(text), "%d %s", hidden, tr("more provinces hidden", "个行省未显示"));
        draw_text_line(hdc, x + 8, y, text, RGB(160, 171, 180));
        y += 22;
    }
    return y;
}

void draw_diplomacy_tab(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    int civ_id = displayed_civ_id();
    int y = TOP_BAR_H + 58;
    int inner_w = side_panel_w - FORM_X_PAD * 2;
    int quad_w = (inner_w - 24) / 4;
    int metric_h = 28;
    int relations = 0;
    int hidden_relations = 0;
    int i;
    char text[512];
    const char *tooltip_text = NULL;

    if (civ_id < 0 || civ_id >= civ_count) {
        SelectObject(hdc, title_font);
        draw_text_line(hdc, x, y, tr("No Country Selected", "未选择国家"), RGB(245, 245, 245));
        SelectObject(hdc, body_font);
        draw_text_line(hdc, x, y + 30, tr("Select a country or one of its tiles.", "选择国家或它的地块。"), RGB(180, 190, 198));
        return;
    }

    SelectObject(hdc, title_font);
    {
        RECT swatch = {x, y + 3, x + 18, y + 21};
        fill_rect(hdc, swatch, civs[civ_id].color);
        snprintf(text, sizeof(text), "%c  %.63s", civs[civ_id].symbol, civs[civ_id].name);
        draw_text_line(hdc, x + 28, y, text, RGB(245, 245, 245));
    }
    y += 26;
    SelectObject(hdc, body_font);
    snprintf(text, sizeof(text), "%s: %s", tr("Capital", "首都"), capital_name_for_civ(civ_id));
    draw_text_line(hdc, x + 28, y, text, RGB(180, 190, 198));
    y += 34;

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, y, tr("Known Relations", "已接触关系"), RGB(245, 245, 245));
    y += 28;
    SelectObject(hdc, body_font);
    for (i = 0; i < civ_count; i++) {
        DiplomacyRelation relation;
        ActiveWar active_war;
        RECT swatch;
        RECT badge;

        if (i == civ_id) continue;
        relation = diplomacy_relation(civ_id, i);
        if (relation.state == DIPLOMACY_NONE) continue;
        if (y > client.bottom - 330) {
            hidden_relations++;
            continue;
        }
        swatch.left = x + 2;
        swatch.top = y + 4;
        swatch.right = swatch.left + 14;
        swatch.bottom = swatch.top + 14;
        fill_rect(hdc, swatch, civs[i].color);
        badge.left = client.right - side_panel_w + FORM_X_PAD + 318;
        badge.top = y - 1;
        badge.right = client.right - FORM_X_PAD;
        badge.bottom = y + 23;
        snprintf(text, sizeof(text), "%c %.42s   %s %d",
                 civs[i].symbol, civs[i].name, metric_label("REL", "关系"), relation.relation_score);
        draw_text_line(hdc, x + 24, y, text, RGB(230, 235, 240));
        draw_status_badge(hdc, badge, relation.state);
        y += 24;
        {
            RECT m = metric_grid_rect(x, y, quad_w, metric_h, 0);
            draw_metric_box(hdc, m, ICON_CULTURE, metric_label("REL", "关系"), relation.relation_score,
                            diplomacy_status_color(relation.state), tr("Relationship score", "关系度"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 1);
            draw_metric_box(hdc, m, ICON_ATTACK, metric_label("TEN", "紧张"), relation.border_tension,
                            RGB(196, 154, 72), tr("Border and pressure tension", "边界和压力造成的紧张度"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 2);
            draw_metric_box(hdc, m, ICON_ECONOMY, metric_label("TRD", "贸易"), relation.trade_fit,
                            RGB(169, 134, 54), tr("Trade fit", "贸易契合度"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 3);
            draw_metric_box(hdc, m, ICON_DISORDER, metric_label("CON", "冲突"), relation.resource_conflict,
                            RGB(176, 72, 62), tr("Resource conflict", "资源冲突"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 4);
            draw_metric_box(hdc, m, ICON_TERRITORY, metric_label("BOR", "边界"), relation.border_length,
                            RGB(88, 137, 83), tr("Shared border length", "接壤边界长度"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 5);
            draw_metric_box(hdc, m, ICON_GEOGRAPHY, metric_label("BAR", "屏障"), relation.natural_barrier,
                            RGB(82, 114, 153), tr("Natural barrier between countries", "国家之间的天然屏障"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 6);
            draw_metric_box(hdc, m, ICON_CLIMATE, metric_label("YRS", "已知"), relation.years_known,
                            RGB(102, 128, 180), tr("Years known", "已接触年数"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 7);
            draw_metric_box(hdc, m, ICON_COUNTRY_DEFENSE, metric_label("TRC", "停战"), relation.truce_years_left,
                            RGB(128, 148, 170), tr("Truce years left", "剩余停战年数"), &tooltip_text);
            y += 2 * (metric_h + 6) + 4;
        }
        if (relation.state == DIPLOMACY_VASSAL && relation.overlord >= 0 && relation.overlord < civ_count) {
            snprintf(text, sizeof(text), "%s: %s", tr("Overlord", "宗主"), civs[relation.overlord].name);
            draw_text_line(hdc, x + 24, y, text, RGB(190, 176, 210));
            y += 20;
        }
        active_war = war_state_between(civ_id, i);
        if (active_war.active && y < client.bottom - 368) {
            RECT bar = {x + 24, y + 2, client.right - FORM_X_PAD, y + 28};
            draw_war_progress_bar(hdc, bar, civ_id, i, active_war);
            y += 34;
        }
        relations++;
    }
    if (relations == 0) {
        draw_text_line(hdc, x + 8, y, tr("No contacted civilizations yet.", "还没有已接触文明。"), RGB(160, 171, 180));
        y += 28;
    }
    if (hidden_relations > 0) {
        snprintf(text, sizeof(text), "%d %s", hidden_relations, tr("more relations hidden", "个关系未显示"));
        draw_text_line(hdc, x + 8, y, text, RGB(160, 171, 180));
        y += 26;
    }
    y += 10;
    draw_military_section(hdc, client, x, y, civ_id, title_font, body_font);
    draw_tooltip(hdc, client, tooltip_text);
}
