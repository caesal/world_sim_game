#ifndef WORLD_SIM_RENDER_SNAPSHOT_PLAGUE_TYPES_H
#define WORLD_SIM_RENDER_SNAPSHOT_PLAGUE_TYPES_H

#include "core/constants.h"
#include "core/value_types.h"

#include <stdint.h>

#define RENDER_SNAPSHOT_PLAGUE_TOP_CITY_COUNT 5

typedef enum {
    SNAPSHOT_PLAGUE_IMPACT_NONE = 0,
    SNAPSHOT_PLAGUE_IMPACT_ACTIVE,
    SNAPSHOT_PLAGUE_IMPACT_RECOVERED,
    SNAPSHOT_PLAGUE_IMPACT_NO_LONGER_EXISTS
} SnapshotPlagueImpactStatus;

typedef struct {
    int civ_id;
    int civ_uid;
    int alive;
    char symbol;
    Color32 color;
    char name_en[NAME_LEN];
    char name_zh[NAME_LEN];
    int64_t current_population;
    int current_infected_city_count;
    int ever_infected_city_count;
    int64_t episode_deaths;
    int death_share_basis_points;
    int peak_severity;
    SnapshotPlagueImpactStatus status;
} SnapshotPlagueCountryImpact;

typedef struct {
    int city_id;
    char city_name[NAME_LEN];
    int current_owner_id;
    int current_owner_uid;
    int current_owner_alive;
    char current_owner_symbol;
    Color32 current_owner_color;
    char current_owner_name_en[NAME_LEN];
    char current_owner_name_zh[NAME_LEN];
    int64_t current_population;
    int64_t episode_deaths;
    int months_remaining;
    int generation;
    SnapshotPlagueImpactStatus status;
} SnapshotPlagueCityImpact;

typedef struct {
    int active;
    int episode_id;
    int country_count;
    int city_count;
    int candidate_city_count;
    int unattributed_city_count;
    int64_t unattributed_deaths;
    int country_sort_comparison_count;
    unsigned int refresh_count;
    int64_t total_episode_deaths;
    SnapshotPlagueCountryImpact countries[MAX_CIVS];
    SnapshotPlagueCityImpact cities[RENDER_SNAPSHOT_PLAGUE_TOP_CITY_COUNT];
} SnapshotPlagueImpact;

#endif
