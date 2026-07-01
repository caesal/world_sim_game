#ifndef WORLD_SIM_GAME_PLAYER_ACTIONS_H
#define WORLD_SIM_GAME_PLAYER_ACTIONS_H

typedef enum {
    GAME_PLAYER_ACTION_OK = 0,
    GAME_PLAYER_ACTION_INVALID_SOURCE,
    GAME_PLAYER_ACTION_INVALID_TARGET,
    GAME_PLAYER_ACTION_SELF_TARGET,
    GAME_PLAYER_ACTION_SOURCE_NOT_SOVEREIGN,
    GAME_PLAYER_ACTION_NO_CONTACT,
    GAME_PLAYER_ACTION_ALREADY_ACTIVE,
    GAME_PLAYER_ACTION_NO_ACTIVE_WAR,
    GAME_PLAYER_ACTION_STABILITY_REORGANIZING,
    GAME_PLAYER_ACTION_STABILITY_CAUTION,
    GAME_PLAYER_ACTION_TARGET_IS_OWN_VASSAL,
    GAME_PLAYER_ACTION_TARGET_IS_VASSAL,
    GAME_PLAYER_ACTION_TARGET_OVERLORD_ALREADY_AT_WAR,
    GAME_PLAYER_ACTION_REDIRECTED_TO_SELF,
    GAME_PLAYER_ACTION_NO_FRONT_AFTER_REDIRECT,
    GAME_PLAYER_ACTION_WAR_SLOT_FULL,
    GAME_PLAYER_ACTION_TARGET_IS_ALLY,
    GAME_PLAYER_ACTION_OK_BROKE_ALLIANCE,
    GAME_PLAYER_ACTION_ALREADY_ALLIED,
    GAME_PLAYER_ACTION_DIFFERENT_ALLIANCES,
    GAME_PLAYER_ACTION_NO_ALLIANCES,
    GAME_PLAYER_ACTION_ALLIANCE_SLOT_FULL,
    GAME_PLAYER_ACTION_VASSAL_ALLIANCE_BLOCKED,
    GAME_PLAYER_ACTION_ALLIANCE_BLOCKED,
    GAME_PLAYER_ACTION_TARGET_ALREADY_IN_ALLIANCE,
    GAME_PLAYER_ACTION_TARGET_NOT_ALLIANCE_MEMBER,
    GAME_PLAYER_ACTION_RULE_BLOCKED
} GamePlayerActionResult;

GamePlayerActionResult game_player_declare_war(int source_civ, int target_civ);
GamePlayerActionResult game_player_peace_all(int source_civ);
GamePlayerActionResult game_player_form_alliance(int source_civ, int target_civ);
GamePlayerActionResult game_player_dissolve_alliances(int source_civ);
GamePlayerActionResult game_player_leave_alliance(int source_civ);
GamePlayerActionResult game_player_alliance_invite(int alliance_id, int target_civ);
GamePlayerActionResult game_player_alliance_remove(int alliance_id, int target_civ);
GamePlayerActionResult game_player_vassalize(int source_civ, int target_civ);

#endif
