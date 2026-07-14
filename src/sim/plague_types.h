#ifndef WORLD_SIM_PLAGUE_TYPES_H
#define WORLD_SIM_PLAGUE_TYPES_H

#include "core/constants.h"
#include "core/value_types.h"

#include <stdint.h>

#define PLAGUE_NAME_COUNT 100
#define PLAGUE_RECENT_HISTORY_CAP 16
#define PLAGUE_VIEW_HISTORY_COUNT 7
#define PLAGUE_ROLLING_START_CAP 3
#define PLAGUE_DEATH_WINDOW_MONTHS 12
#define PLAGUE_CIV_WORD_COUNT ((MAX_CIVS + 63) / 64)
#define PLAGUE_MORTALITY_Q32_ONE (UINT64_C(1) << 32)

typedef enum {
    PLAGUE_SIZE_NONE = 0,
    PLAGUE_SIZE_SMALL,
    PLAGUE_SIZE_MEDIUM,
    PLAGUE_SIZE_LARGE
} PlagueSize;

typedef enum {
    PLAGUE_PROBABILITY_NO_PLAGUE = 0,
    PLAGUE_PROBABILITY_SMALL,
    PLAGUE_PROBABILITY_MEDIUM,
    PLAGUE_PROBABILITY_LARGE,
    PLAGUE_PROBABILITY_COUNT
} PlagueProbabilityBucket;

typedef struct {
    int no_plague;
    int small;
    int medium;
    int large;
} PlagueProbabilityDistribution;

typedef enum {
    PLAGUE_ACTION_NONE = 0,
    PLAGUE_ACTION_SPREAD,
    PLAGUE_ACTION_PERSIST
} PlagueAction;

typedef enum {
    PLAGUE_ROUTE_LAND = 0,
    PLAGUE_ROUTE_SHALLOW,
    PLAGUE_ROUTE_DEEP,
    PLAGUE_ROUTE_COUNT
} PlagueRouteType;

typedef struct {
    int spore_percent;
    int maximum_generation;
    int continuous_city_cap_months;
    int severity_min;
    int severity_max;
    int spread_weight;
    int persist_weight;
} PlagueSizeRules;

typedef struct {
    int spread;
    int persist;
    int total;
} PlagueActionWeights;

typedef struct {
    int weight[PLAGUE_ROUTE_COUNT];
    int total;
} PlagueRouteWeights;

typedef struct {
    int active;
    int ever_infected_current_episode;
    int generation;
    int infection_start_month;
    int recovery_month;
    int next_pulse_month;
    int immunity_percent;
    int immunity_expiry_month;
    int64_t episode_deaths;
    uint64_t mortality_carry_q32;
} PlagueCityEpisodeState;

typedef struct {
    int active;
    int episode_id;
    PlagueSize size;
    int severity;
    int name_id;
    int name_cycle;
    int origin_city_id;
    int origin_civ_id;
    int origin_civ_uid;
    char origin_city_name[NAME_LEN];
    char origin_civ_name_en[NAME_LEN];
    char origin_civ_name_zh[NAME_LEN];
    char origin_civ_symbol;
    Color32 origin_civ_color;
    int start_month;
    int frozen_occupied_cities;
    int spores_initial;
    int spores_remaining;
    int active_city_count;
    int ever_infected_city_count;
    int current_generation;
    int maximum_generation_reached;
    int current_affected_country_count;
    int ever_affected_country_count;
    int64_t current_month_deaths;
    int64_t total_deaths;
    uint64_t current_country_bits[PLAGUE_CIV_WORD_COUNT];
    uint64_t ever_country_bits[PLAGUE_CIV_WORD_COUNT];
    int start_event_emitted;
    int end_event_emitted;
} PlagueEpisodeState;

typedef struct {
    int episode_id;
    PlagueSize size;
    int severity;
    int name_id;
    int name_cycle;
    int origin_city_id;
    int origin_civ_id;
    int origin_civ_uid;
    char origin_city_name[NAME_LEN];
    char origin_civ_name_en[NAME_LEN];
    char origin_civ_name_zh[NAME_LEN];
    char origin_civ_symbol;
    Color32 origin_civ_color;
    int start_month;
    int end_month;
    int duration_months;
    int spores_initial;
    int spores_used;
    int infected_city_count;
    int affected_country_count;
    int64_t total_deaths;
} PlagueEpisodeHistory;

typedef struct {
    int initialized;
    int next_episode_id;
    PlagueProbabilityDistribution effective_probabilities;
    PlagueProbabilityDistribution pending_probabilities;
    int pending_probabilities_valid;
    PlagueEpisodeState episode;
    PlagueCityEpisodeState cities[MAX_CITIES];
    int active_city_ids[MAX_CITIES];
    int active_city_count;
    int ever_infected_city_ids[MAX_CITIES];
    int ever_infected_city_count;
    unsigned char ever_infected_flags[MAX_CITIES];
    int rolling_start_months[PLAGUE_ROLLING_START_CAP];
    int rolling_start_count;
    uint64_t used_name_bits[2];
    int name_cycle;
    int death_month_keys[PLAGUE_DEATH_WINDOW_MONTHS];
    int64_t deaths_by_month[PLAGUE_DEATH_WINDOW_MONTHS];
    int64_t deaths_by_month_civ[PLAGUE_DEATH_WINDOW_MONTHS][MAX_CIVS];
    PlagueEpisodeHistory history[PLAGUE_RECENT_HISTORY_CAP];
    int history_head;
    int history_count;
} PlagueModelState;

typedef struct {
    PlagueEpisodeState episode;
    PlagueProbabilityDistribution effective_probabilities;
    PlagueProbabilityDistribution pending_probabilities;
    int pending_probabilities_valid;
    int active_city_count;
    int active_city_ids[MAX_CITIES];
    int current_country_count;
    int current_country_ids[MAX_CIVS];
    int next_scheduled_check_month;
    int starts_in_rolling_window;
    int64_t rolling_12_month_deaths;
    int immunity_city_count[4];
    int disorder_current;
    int disorder_target;
    int projected_immunity_percent;
    int next_immunity_percent;
    int months_to_next_immunity;
    int maximum_generation_index;
    int reachable_generation_layers;
    int recent_history_count;
    PlagueEpisodeHistory recent_history[PLAGUE_VIEW_HISTORY_COUNT];
} PlagueStateView;

typedef struct {
    int source_city_id;
    int target_city_id;
    PlagueAction action;
    PlagueRouteType route_type;
    int lane_id;
    int proposed_generation;
    uint32_t priority;
} PlaguePulseRequest;

typedef struct {
    uint64_t step_total_us;
    uint64_t step_peak_us;
    uint64_t step_last_us;
    uint64_t step_samples;
    int active_city_count;
    int due_pulse_count;
    int candidate_edge_count;
    int candidate_edge_count_by_route[PLAGUE_ROUTE_COUNT];
    int pending_request_count;
    int committed_infection_count;
    int committed_persistence_count;
    int deduplicated_request_count;
    int adjacency_revision;
    int adjacency_rebuild_count;
    int adjacency_last_rebuild_us;
    int spores_remaining;
    int episode_id;
} PlagueMetricsSnapshot;

#endif
