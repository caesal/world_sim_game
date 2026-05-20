#ifndef WORLD_SIM_RENDER_SNAPSHOT_H
#define WORLD_SIM_RENDER_SNAPSHOT_H

#include "core/game_types.h"
#include "sim/decision_snapshot.h"
#include "sim/sea_lanes.h"

#define RENDER_SNAPSHOT_EVENT_COUNT 200

enum {
    RENDER_SNAPSHOT_SECTION_TILES = 1 << 0,
    RENDER_SNAPSHOT_SECTION_CIVS = 1 << 1,
    RENDER_SNAPSHOT_SECTION_CITIES = 1 << 2,
    RENDER_SNAPSHOT_SECTION_REGIONS = 1 << 3,
    RENDER_SNAPSHOT_SECTION_LANES = 1 << 4,
    RENDER_SNAPSHOT_SECTION_PLAGUE = 1 << 5,
    RENDER_SNAPSHOT_SECTION_EVENTS = 1 << 6,
    RENDER_SNAPSHOT_SECTION_DIPLOMACY = 1 << 7
};

typedef struct {
    unsigned char geography;
    unsigned char climate;
    unsigned char ecology;
    unsigned char resource;
    unsigned char river;
    unsigned char elevation;
    unsigned char water_depth;
    unsigned char water_deep_percent;
    short moisture;
    short temperature;
    short owner;
    short region_id;
    short province_id;
} SnapshotTile;

typedef struct {
    int alive;
    int id;
    int uid;
    Color32 color;
    char symbol;
    int population;
    int army;
    int current_soldiers;
    int aggression;
    int expansion;
    int defense;
    int culture;
    int governance;
    int cohesion;
    int production;
    int military;
    int commerce;
    int logistics;
    int innovation;
    int adaptation;
    int tech_stage;
    int tech_progress;
    int tech_stage_progress_percent;
    int tech_months_to_next;
    int tech_required_months;
    int tech_expansion_percent;
    int tech_resource_percent;
    int tech_progress_percent;
    int disorder;
    int disorder_resource;
    int disorder_plague;
    int disorder_migration;
    int disorder_stability;
    int disorder_last_pressure;
    int disorder_last_recovery;
    int disorder_last_net;
    int disorder_last_pressure_x10;
    int disorder_last_recovery_x10;
    int disorder_last_net_x10;
    int disorder_last_base_recovery_x10;
    int disorder_last_governance_recovery_x10;
    int disorder_last_cohesion_recovery_x10;
    int disorder_last_peace_recovery_x10;
    int disorder_last_condition_recovery_x10;
    int disorder_last_plague_decay;
    int disorder_last_war_decay;
    int disorder_last_migration_decay;
    int collapse_grace_months;
    int plague_random_immunity_months;
    int plague_active_count;
    int plague_months_left;
    int plague_peak_severity;
    int plague_deaths_total;
    int war_active;
    int war_deployed_soldiers;
    int war_available_reserve;
    int war_front_count;
    int vassal_governance_disorder;
    int vassal_callable_soldiers;
    int vassal_resource_tribute;
    int decision_expansion_weight;
    int decision_war_weight;
    int decision_stability_weight;
    int decision_next_expansion_months;
    int collapse_can_trigger;
    int collapse_block_reason;
    int capital_city;
    int overlord;
    int vassal_count;
    int name_id;
    CountrySummary summary;
    PopulationSummary population_summary;
    char main_intent[32];
    char decision_expansion_reason[128];
    char decision_war_reason[128];
    DecisionSnapshot decision;
    char collapse_last_reason[EVENT_LOG_LEN];
    char name_en[NAME_LEN];
    char name_zh[NAME_LEN];
} SnapshotCiv;

typedef struct {
    int state;
    int relation_score;
    int border_tension;
    int trade_fit;
    int resource_conflict;
    int truce_years_left;
    int truce_initial_years;
    int border_length;
    int natural_barrier;
    int years_known;
    int vassal_years;
    int easing_years;
    int contact_kind;
    int years_distant_known;
    int overlord;
    int vassal;
    int last_war_winner;
    int last_war_loser;
    int last_war_result;
} SnapshotDiplomacyRelation;

typedef struct {
    int active;
    int attacker;
    int defender;
    int soldiers_a;
    int soldiers_b;
    int casualties_a;
    int casualties_b;
    int support_casualties_a;
    int support_casualties_b;
    int wins_a;
    int wins_b;
    int years;
} SnapshotWar;

typedef struct {
    int alive;
    int owner;
    int x;
    int y;
    int population;
    int radius;
    int capital;
    int port;
    int port_x;
    int port_y;
    int port_region;
    int plague_active;
    int plague_severity;
    int plague_months_left;
    int plague_deaths_total;
    RegionSummary region_summary;
    PopulationSummary population_summary;
    char name[NAME_LEN];
} SnapshotCity;

typedef struct {
    int alive;
    int owner;
    int center_x;
    int center_y;
    int tile_count;
    int city_id;
    int capital_x;
    int capital_y;
    int port_x;
    int port_y;
    int has_port_site;
    int development_score;
    int natural_defense;
    int cradle_score;
    Geography dominant_geography;
    Climate dominant_climate;
    Ecology dominant_ecology;
    TerrainStats average_stats;
    int name_id;
    char name_en[96];
    char name_zh[96];
} SnapshotRegion;

typedef struct {
    int active;
    int type;
    int from_node;
    int to_node;
    int from_region;
    int to_region;
    int from_city;
    int to_city;
    MapPoint from_port;
    MapPoint to_port;
    MapPoint from_sea_entry;
    MapPoint to_sea_entry;
    int point_count;
    MapPoint points[MAX_SEA_LANE_POINTS];
    int exposure;
} SnapshotSeaLane;

typedef struct {
    EventLogEntry entry;
    EventLogType type;
    char text_en[EVENT_LOG_LEN * 3];
    char text_zh[EVENT_LOG_LEN * 3];
} SnapshotEvent;

typedef struct {
    int map_w;
    int map_h;
    int year;
    int month;
    int world_generated;
    int civ_count;
    int civ_alive_count;
    int civ_reusable_slot_count;
    int city_count;
    int region_count;
    int lane_count;
    int event_count;
    int event_total_entries;
    int tiles_revision;
    int civs_revision;
    int cities_revision;
    int regions_revision;
    int diplomacy_revision;
    int lanes_revision;
    int plague_revision;
    int events_revision;
    int sections_copied_mask;
    int sections_skipped_mask;
    unsigned int revision;
    SnapshotTile tiles[MAX_MAP_W * MAX_MAP_H];
    SnapshotCiv civs[MAX_CIVS];
    SnapshotCity cities[MAX_CITIES];
    SnapshotRegion regions[MAX_NATURAL_REGIONS];
    SnapshotSeaLane lanes[MAX_SEA_LANES];
    SnapshotDiplomacyRelation relations[MAX_CIVS][MAX_CIVS];
    SnapshotWar wars[MAX_CIVS][MAX_CIVS];
    int war_front_flags[MAX_CIVS][MAX_CIVS];
    int war_peace_pressure[MAX_CIVS][MAX_CIVS];
    int plague_city_severity[MAX_CITIES];
    int plague_lane_exposure[MAX_SEA_LANES];
    int plague_active;
    SnapshotEvent events[RENDER_SNAPSHOT_EVENT_COUNT];
    SnapshotEvent civ_recent_events[MAX_CIVS][EVENT_LOG_CIV_HISTORY_COUNT];
    int civ_recent_event_count[MAX_CIVS];
} RenderSnapshot;

void render_snapshot_init(void);
void render_snapshot_shutdown(void);
void render_snapshot_publish_from_live_state(void);
int render_snapshot_publish_from_live_state_throttled(int force);
const RenderSnapshot *render_snapshot_acquire(void);
void render_snapshot_release(const RenderSnapshot *snapshot);
unsigned int render_snapshot_revision(void);
int render_snapshot_age_ms(void);
int render_snapshot_last_publish_ms(void);
int render_snapshot_skipped_publish_count(void);
int render_snapshot_throttled_publish_count(void);
const char *render_snapshot_last_skip_reason(void);

const SnapshotTile *render_snapshot_tile_at(const RenderSnapshot *snapshot, int x, int y);
const char *render_snapshot_event_text(const RenderSnapshot *snapshot, int index, int language);
int render_snapshot_event_get_entry(const RenderSnapshot *snapshot, int index, EventLogEntry *out);
EventLogType render_snapshot_event_get_type(const RenderSnapshot *snapshot, int index);
int render_snapshot_civ_recent_event_count(const RenderSnapshot *snapshot, int civ_id);
int render_snapshot_civ_recent_event_get_entry(const RenderSnapshot *snapshot, int civ_id,
                                               int index, EventLogEntry *out);

#endif
