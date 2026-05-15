#ifndef WORLD_SIM_RENDER_SNAPSHOT_H
#define WORLD_SIM_RENDER_SNAPSHOT_H

#include "core/game_types.h"
#include "sim/sea_lanes.h"

#define RENDER_SNAPSHOT_EVENT_COUNT 200

typedef struct {
    unsigned char geography;
    unsigned char climate;
    unsigned char river;
    unsigned char elevation;
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
    int tech_stage;
    int tech_progress;
    int disorder;
    int capital_city;
    int overlord;
    int vassal_count;
    int name_id;
    char name_en[NAME_LEN];
    char name_zh[NAME_LEN];
} SnapshotCiv;

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
    char name[NAME_LEN];
} SnapshotCity;

typedef struct {
    int alive;
    int owner;
    int center_x;
    int center_y;
    int tile_count;
    int city_id;
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
    int city_count;
    int region_count;
    int lane_count;
    int event_count;
    int event_total_entries;
    int tiles_revision;
    int lanes_revision;
    int plague_revision;
    int events_revision;
    unsigned int revision;
    SnapshotTile tiles[MAX_MAP_W * MAX_MAP_H];
    SnapshotCiv civs[MAX_CIVS];
    SnapshotCity cities[MAX_CITIES];
    SnapshotRegion regions[MAX_NATURAL_REGIONS];
    SnapshotSeaLane lanes[MAX_SEA_LANES];
    int plague_city_severity[MAX_CITIES];
    int plague_lane_exposure[MAX_SEA_LANES];
    int plague_active;
    SnapshotEvent events[RENDER_SNAPSHOT_EVENT_COUNT];
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

#endif
