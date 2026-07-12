#ifndef WORLD_SIM_WORLD_ANNOUNCEMENT_TYPES_H
#define WORLD_SIM_WORLD_ANNOUNCEMENT_TYPES_H

#include "core/constants.h"
#include "core/value_types.h"

#define WORLD_ANNOUNCEMENT_ALLIANCE_NAME_LEN 96

typedef enum {
    WORLD_ANNOUNCEMENT_NORMAL = 1,
    WORLD_ANNOUNCEMENT_MAJOR = 2,
    WORLD_ANNOUNCEMENT_CRITICAL = 3
} WorldAnnouncementPriority;

typedef enum {
    WORLD_ANNOUNCEMENT_WAR_NONE,
    WORLD_ANNOUNCEMENT_WAR_ALLIANCE_VS_COUNTRY,
    WORLD_ANNOUNCEMENT_WAR_ALLIANCE_VS_ALLIANCE
} WorldAnnouncementWarClass;

typedef enum {
    WORLD_ANNOUNCEMENT_TERMINAL_NONE,
    WORLD_ANNOUNCEMENT_TERMINAL_VICTORY,
    WORLD_ANNOUNCEMENT_TERMINAL_OFFENSIVE_HALTED,
    WORLD_ANNOUNCEMENT_TERMINAL_FRONT_SEVERED,
    WORLD_ANNOUNCEMENT_TERMINAL_NEGOTIATED_TRUCE
} WorldAnnouncementTerminalResult;

typedef struct {
    int civ_id;
    int uid;
    char name_en[NAME_LEN];
    char name_zh[NAME_LEN];
    char symbol;
    Color32 color;
} WorldAnnouncementCivIdentity;

typedef struct {
    int valid;
    int alliance_id;
    int leader_civ_id;
    char name_en[WORLD_ANNOUNCEMENT_ALLIANCE_NAME_LEN];
    char name_zh[WORLD_ANNOUNCEMENT_ALLIANCE_NAME_LEN];
    Color32 color;
} WorldAnnouncementAllianceIdentity;

typedef struct {
    int event_id;
    int event_type;
    int priority;
    int year;
    int month;
    WorldAnnouncementCivIdentity actor;
    WorldAnnouncementCivIdentity target;
    WorldAnnouncementAllianceIdentity alliance_a;
    WorldAnnouncementAllianceIdentity alliance_b;
    int war_id;
    int war_class;
    int terminal_result;
    int winner_side;
    int technology_stage;
    int related_count;
    WorldAnnouncementCivIdentity related[MAX_CIVS];
    int location_civ_id;
    int location_civ_uid;
    int location_city_id;
    int location_region_id;
    int location_x;
    int location_y;
    char location_name_en[NAME_LEN];
    char location_name_zh[NAME_LEN];
} WorldAnnouncementEvent;

typedef struct {
    int event_id;
    int event_type;
    int priority;
    int year;
    int month;
} WorldAnnouncementStreamEntry;

#endif
