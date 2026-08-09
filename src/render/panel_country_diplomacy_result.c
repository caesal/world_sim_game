#include "render/panel_country_diplomacy_result.h"

#include "sim/diplomacy.h"
#include "ui/ui_theme.h"

static int result_has_winner(int result) {
    return result == DIP_LAST_WAR_DECISIVE ||
           result == DIP_LAST_WAR_MILITARY ||
           result == DIP_LAST_WAR_SURRENDER;
}

const char *panel_country_diplomacy_result_text(int local_identity,
                                                int winner_identity,
                                                int loser_identity,
                                                int result) {
    if (result == DIP_LAST_WAR_INTERRUPTED ||
        result == DIP_LAST_WAR_FRONT_SEVERED) {
        return tr("Front Severed", "战线中断");
    }
    if (result == DIP_LAST_WAR_NEGOTIATED_TRUCE) {
        return tr("Negotiated Truce", "议和停战");
    }
    if (result == DIP_LAST_WAR_OFFENSIVE_HALTED) {
        if (winner_identity == local_identity) return tr("Offensive Halted", "攻势中止");
        if (loser_identity == local_identity) return tr("Enemy Offensive Halted", "对方攻势中止");
        return tr("Offensive Halted", "攻势中止");
    }
    if (result == DIP_LAST_WAR_SURRENDER) {
        if (winner_identity == local_identity) return tr("Surrender Win", "受降胜利");
        if (loser_identity == local_identity) return tr("Surrender", "投降战败");
    }
    if (result == DIP_LAST_WAR_DECISIVE || result == DIP_LAST_WAR_MILITARY) {
        if (winner_identity == local_identity) return tr("Military Win", "军事胜利");
        if (loser_identity == local_identity) return tr("Military Defeat", "军事战败");
    }
    return "-";
}

COLORREF panel_country_diplomacy_result_color(int local_identity,
                                              int winner_identity,
                                              int loser_identity,
                                              int result) {
    if (result_has_winner(result)) {
        if (winner_identity == local_identity) return ui_theme_color(UI_COLOR_GOOD);
        if (loser_identity == local_identity) return ui_theme_color(UI_COLOR_DANGER);
    }
    return ui_theme_color(UI_COLOR_ACCENT);
}
