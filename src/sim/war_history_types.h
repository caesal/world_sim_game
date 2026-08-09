#ifndef WORLD_SIM_WAR_HISTORY_TYPES_H
#define WORLD_SIM_WAR_HISTORY_TYPES_H

#include "core/constants.h"
#include "core/value_types.h"

#include <stdint.h>

#define WAR_HISTORY_CAPACITY 3
#define WAR_HISTORY_INVALID_UID 0

typedef struct {
    int uid;
    char name_en[NAME_LEN];
    char name_zh[NAME_LEN];
    Color32 color;
} WarHistoryPrincipal;

typedef struct {
    uint64_t war_serial;
    WarHistoryPrincipal local;
    WarHistoryPrincipal opponent;
    int result;
    int winner_uid;
    int loser_uid;
    int local_casualties;
    int opponent_casualties;
    int transferred_regions;
    int indemnity_paid;
    int beneficiary_uid;
    int end_year;
    int end_month;
    int duration_months;
} WarHistoryRecord;

typedef struct {
    int owner_uid;
    int count;
    uint64_t revision;
    WarHistoryRecord records[WAR_HISTORY_CAPACITY];
} WarHistory;

typedef struct {
    int winner_civ;
    int loser_civ;
    int beneficiary_civ;
    int transferred_regions;
    int indemnity_paid;
    int indemnity_distributed;
    int forced_alliance_exit;
} WarSettlementResult;

typedef struct {
    uint64_t next_serial;
    uint64_t global_revision;
    WarHistory histories[MAX_CIVS];
} WarHistorySaveState;

#endif
