#include "ui/ui_country_target.h"

#include "core/game_state.h"
#include "core/load_progress.h"
#include "core/render_snapshot.h"
#include "core/worldgen_progress.h"
#include "game/game_loop.h"
#include "game/game_player_actions.h"
#include "ui/color_picker.h"
#include "ui/pause_menu.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_layout.h"
#include "ui/ui_map_input.h"
#include "ui/ui_notifications.h"
#include "ui/ui_snapshot_read.h"

typedef struct {
    UiCountryTargetMode mode;
    int source_civ;
    int previous_auto_run;
    int mouse_x;
    int mouse_y;
} UiCountryTargetState;

static UiCountryTargetState target_state = {UI_COUNTRY_TARGET_NONE, -1, 0, -1, -1};

static void notify(HWND hwnd, const char *en, const char *zh) {
    ui_notifications_push(en, zh);
    ui_notifications_invalidate(hwnd);
}

static int source_has_capital(int source_civ) {
    const RenderSnapshot *snapshot = render_snapshot_acquire();
    int ok = 0;
    if (snapshot && source_civ >= 0 && source_civ < snapshot->civ_count) {
        const SnapshotCiv *civ = &snapshot->civs[source_civ];
        if (civ->alive && civ->capital_city >= 0 && civ->capital_city < snapshot->city_count) {
            const SnapshotCity *city = &snapshot->cities[civ->capital_city];
            ok = city->alive && city->owner == source_civ;
        }
    }
    render_snapshot_release(snapshot);
    return ok;
}

static int target_mode_blocked(void) {
    WorldGenProgress progress;
    worldgen_progress_get(&progress);
    return !world_generated || load_progress_active() || progress.active ||
           pause_menu_open || color_picker_active();
}

static void restore_auto_run(void) {
    auto_run = target_state.previous_auto_run ? 1 : 0;
    game_loop_reset();
}

static void clamp_target_mouse_to_map(HWND hwnd) {
    RECT client;
    RECT map_rect;
    if (!GetClientRect(hwnd, &client)) return;
    map_rect = get_map_viewport_rect(client);
    if (target_state.mouse_x < map_rect.left) target_state.mouse_x = map_rect.left;
    if (target_state.mouse_x >= map_rect.right) target_state.mouse_x = map_rect.right - 1;
    if (target_state.mouse_y < map_rect.top) target_state.mouse_y = map_rect.top;
    if (target_state.mouse_y >= map_rect.bottom) target_state.mouse_y = map_rect.bottom - 1;
}

static void initialize_target_mouse(HWND hwnd, int mouse_x, int mouse_y) {
    POINT point;
    if (mouse_x >= 0 && mouse_y >= 0) {
        target_state.mouse_x = mouse_x;
        target_state.mouse_y = mouse_y;
        clamp_target_mouse_to_map(hwnd);
        return;
    }
    if (GetCursorPos(&point) && ScreenToClient(hwnd, &point)) {
        target_state.mouse_x = point.x;
        target_state.mouse_y = point.y;
        clamp_target_mouse_to_map(hwnd);
    }
}

static int begin_target(HWND hwnd, int source_civ, UiCountryTargetMode mode,
                        int mouse_x, int mouse_y) {
    if (target_mode_blocked()) return 0;
    if (!ui_snapshot_civ_alive(source_civ)) {
        notify(hwnd, "Select a living source country.", "请选择存活的源国家。");
        return 0;
    }
    if (!source_has_capital(source_civ)) {
        notify(hwnd, "Source country has no valid capital.", "源国家没有有效首都。");
        return 0;
    }
    target_state.mode = mode;
    target_state.source_civ = source_civ;
    target_state.previous_auto_run = auto_run ? 1 : 0;
    initialize_target_mouse(hwnd, mouse_x, mouse_y);
    auto_run = 0;
    game_loop_reset();
    ui_invalidate_game_redraw(hwnd, GAME_REDRAW_TOP_BAR | GAME_REDRAW_BOTTOM_BAR |
                                    GAME_REDRAW_MAP_DYNAMIC);
    return 1;
}

static void complete_target(HWND hwnd) {
    restore_auto_run();
    target_state.mode = UI_COUNTRY_TARGET_NONE;
    target_state.source_civ = -1;
    ui_invalidate_game_redraw(hwnd, GAME_REDRAW_TOP_BAR | GAME_REDRAW_BOTTOM_BAR |
                                    GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_SIDE_PANEL);
}

static void notify_result(HWND hwnd, UiCountryTargetMode mode, GamePlayerActionResult result) {
    if (result == GAME_PLAYER_ACTION_NO_CONTACT && mode == UI_COUNTRY_TARGET_DECLARE_WAR) {
        notify(hwnd, "War requires land border or sea route contact.",
               "开战需要陆地边界或航道联系。");
    } else if (result == GAME_PLAYER_ACTION_NO_CONTACT) {
        notify(hwnd, "Vassalization requires land border or sea route contact.",
               "附庸化需要陆地边界或航道联系。");
    } else if (result == GAME_PLAYER_ACTION_SELF_TARGET) {
        notify(hwnd, "Choose a different country.", "请选择另一个国家。");
    } else if (result == GAME_PLAYER_ACTION_SOURCE_NOT_SOVEREIGN) {
        notify(hwnd, "This country cannot act independently.", "该国家没有独立行动权。");
    } else if (result == GAME_PLAYER_ACTION_ALREADY_ACTIVE) {
        notify(hwnd, "War is already active.", "战争已经进行中。");
    } else if (result == GAME_PLAYER_ACTION_STABILITY_REORGANIZING) {
        notify(hwnd, "This country cannot start a new war while reorganizing.",
               "该国正在重整，不能发动新战争。");
    } else if (result == GAME_PLAYER_ACTION_STABILITY_CAUTION) {
        notify(hwnd, "Cautious stability only allows core-reconnection wars.",
               "谨慎稳定状态只允许重连核心领土的战争。");
    } else if (result == GAME_PLAYER_ACTION_TARGET_IS_OWN_VASSAL) {
        if (mode == UI_COUNTRY_TARGET_DECLARE_WAR) {
            notify(hwnd, "You cannot declare war on your own vassal.",
                   "不能向自己的附庸宣战。");
        } else {
            notify(hwnd, "That country is already your vassal.",
                   "该国已经是你的附庸。");
        }
    } else if (result == GAME_PLAYER_ACTION_TARGET_IS_VASSAL) {
        notify(hwnd, "The target is a vassal; declare war on its overlord.",
               "目标是附庸；请向其宗主国宣战。");
    } else if (result == GAME_PLAYER_ACTION_TARGET_OVERLORD_ALREADY_AT_WAR) {
        notify(hwnd, "War against the target's overlord is already active.",
               "与目标宗主国的战争已经在进行。");
    } else if (result == GAME_PLAYER_ACTION_REDIRECTED_TO_SELF) {
        notify(hwnd, "The target's overlord is your own country.",
               "目标的宗主国就是本国。");
    } else if (result == GAME_PLAYER_ACTION_NO_FRONT_AFTER_REDIRECT) {
        notify(hwnd, "No valid front exists against the target's overlord.",
               "无法与目标宗主国形成有效战线。");
    } else if (result == GAME_PLAYER_ACTION_WAR_SLOT_FULL) {
        notify(hwnd, "No war slot is available.", "没有可用的战争槽位。");
    } else if (result == GAME_PLAYER_ACTION_RULE_BLOCKED) {
        notify(hwnd, "Action is blocked by current war or vassal rules.",
               "当前战争或附庸规则阻止了该行动。");
    } else {
        notify(hwnd, "Action could not be completed under current rules.",
               "当前规则下无法完成该行动。");
    }
}

int ui_country_target_active(void) {
    return target_state.mode != UI_COUNTRY_TARGET_NONE;
}

UiCountryTargetView ui_country_target_view(void) {
    UiCountryTargetView view;
    view.active = ui_country_target_active();
    view.mode = target_state.mode;
    view.source_civ = target_state.source_civ;
    view.mouse_x = target_state.mouse_x;
    view.mouse_y = target_state.mouse_y;
    return view;
}

int ui_country_target_handle_action_button(HWND hwnd, int source_civ,
                                           CountryVassalActionType action,
                                           int mouse_x, int mouse_y) {
    GamePlayerActionResult result;
    if (action == COUNTRY_VASSAL_ACTION_DECLARE_WAR) {
        return begin_target(hwnd, source_civ, UI_COUNTRY_TARGET_DECLARE_WAR, mouse_x, mouse_y);
    }
    if (action == COUNTRY_VASSAL_ACTION_VASSALIZE) {
        return begin_target(hwnd, source_civ, UI_COUNTRY_TARGET_VASSALIZE, mouse_x, mouse_y);
    }
    if (action != COUNTRY_VASSAL_ACTION_PEACE) return 0;

    result = game_player_peace_all(source_civ);
    if (result == GAME_PLAYER_ACTION_OK) {
        notify(hwnd, "Direct wars ended without territorial changes.",
               "直接战争已无割地结束。");
    } else if (result == GAME_PLAYER_ACTION_NO_ACTIVE_WAR) {
        notify(hwnd, "No active wars to end.", "没有可结束的战争。");
    } else {
        notify_result(hwnd, UI_COUNTRY_TARGET_NONE, result);
    }
    ui_invalidate_game_redraw(hwnd, GAME_REDRAW_TOP_BAR | GAME_REDRAW_BOTTOM_BAR |
                                    GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_SIDE_PANEL);
    return 1;
}

int ui_country_target_update_mouse(HWND hwnd, int mouse_x, int mouse_y) {
    if (!ui_country_target_active()) return 0;
    if (target_mode_blocked()) return ui_country_target_cancel(hwnd);
    if (target_state.mouse_x == mouse_x && target_state.mouse_y == mouse_y) return 1;
    target_state.mouse_x = mouse_x;
    target_state.mouse_y = mouse_y;
    ui_invalidate_game_redraw(hwnd, GAME_REDRAW_MAP_DYNAMIC);
    return 1;
}

int ui_country_target_handle_left_click(HWND hwnd, int mouse_x, int mouse_y) {
    int tile_x;
    int tile_y;
    int owner;
    GamePlayerActionResult result;
    if (!ui_country_target_active()) return 0;
    if (target_mode_blocked()) return ui_country_target_cancel(hwnd);
    target_state.mouse_x = mouse_x;
    target_state.mouse_y = mouse_y;
    if (!ui_map_screen_to_tile(hwnd, mouse_x, mouse_y, &tile_x, &tile_y)) {
        notify(hwnd, "Select a map tile owned by another country.",
               "请选择属于其他国家的地图地块。");
        return 1;
    }
    owner = ui_snapshot_tile_owner(tile_x, tile_y);
    if (!ui_snapshot_civ_alive(owner)) {
        notify(hwnd, "Select a living target country.", "请选择存活的目标国家。");
        return 1;
    }
    if (owner == target_state.source_civ) {
        notify_result(hwnd, target_state.mode, GAME_PLAYER_ACTION_SELF_TARGET);
        return 1;
    }
    if (target_state.mode == UI_COUNTRY_TARGET_DECLARE_WAR) {
        result = game_player_declare_war(target_state.source_civ, owner);
    } else {
        result = game_player_vassalize(target_state.source_civ, owner);
    }
    if (result == GAME_PLAYER_ACTION_OK) {
        complete_target(hwnd);
    } else {
        notify_result(hwnd, target_state.mode, result);
        ui_invalidate_game_redraw(hwnd, GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_SIDE_PANEL);
    }
    return 1;
}

int ui_country_target_cancel(HWND hwnd) {
    if (!ui_country_target_active()) return 0;
    restore_auto_run();
    target_state.mode = UI_COUNTRY_TARGET_NONE;
    target_state.source_civ = -1;
    ui_invalidate_game_redraw(hwnd, GAME_REDRAW_TOP_BAR | GAME_REDRAW_BOTTOM_BAR |
                                    GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_SIDE_PANEL);
    return 1;
}
