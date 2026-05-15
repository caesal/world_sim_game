#include "core/render_snapshot.h"

#include "core/dirty_flags.h"
#include "core/state_lock.h"
#include "data/province_names.h"
#include "sim/maritime.h"
#include "sim/plague.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/vassal.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

static RenderSnapshot buffers[3];
static volatile LONG front_index;
static volatile LONG refs[3];
static volatile LONG published_revision;
static volatile LONG last_publish_tick;
static volatile LONG last_publish_ms;
static volatile LONG skipped_publish_count;
static volatile LONG throttled_publish_count;
static volatile LONG last_skip_reason;
static int initialized;

enum {
    SNAPSHOT_SKIP_NONE = 0,
    SNAPSHOT_SKIP_THROTTLED = 1,
    SNAPSHOT_SKIP_NO_BACK_BUFFER = 2
};

#define SNAPSHOT_MIN_INTERVAL_MS 50

static int choose_back_buffer(void) {
    int front = (int)front_index;
    int i;
    for (i = 0; i < 3; i++) {
        if (i != front && refs[i] == 0) return i;
    }
    return -1;
}

static void copy_tiles(RenderSnapshot *snapshot) {
    int x;
    int y;
    for (y = 0; y < map_h; y++) {
        for (x = 0; x < map_w; x++) {
            SnapshotTile *dst = &snapshot->tiles[y * map_w + x];
            Tile *src = &world[y][x];
            dst->geography = (unsigned char)src->geography;
            dst->climate = (unsigned char)src->climate;
            dst->river = (unsigned char)src->river;
            dst->elevation = (unsigned char)clamp(src->elevation, 0, 255);
            dst->owner = (short)src->owner;
            dst->region_id = (short)src->region_id;
            dst->province_id = (short)src->province_id;
        }
    }
}

static void copy_civs(RenderSnapshot *snapshot) {
    int i;
    snapshot->civ_count = clamp(civ_count, 0, MAX_CIVS);
    for (i = 0; i < snapshot->civ_count; i++) {
        SnapshotCiv *dst = &snapshot->civs[i];
        Civilization *src = &civs[i];
        dst->alive = src->alive;
        dst->id = i;
        dst->uid = src->uid;
        dst->color = src->color;
        dst->symbol = src->symbol;
        dst->population = src->population;
        dst->army = src->military;
        dst->tech_stage = src->tech_stage;
        dst->tech_progress = src->tech_progress;
        dst->disorder = src->disorder;
        dst->capital_city = src->capital_city;
        dst->overlord = vassal_overlord(i);
        dst->vassal_count = vassal_direct_count(i);
        dst->name_id = src->name_id;
        snprintf(dst->name_en, sizeof(dst->name_en), "%s",
                 civilization_display_name_for_language(i, 0));
        snprintf(dst->name_zh, sizeof(dst->name_zh), "%s",
                 civilization_display_name_for_language(i, 1));
    }
}

static void copy_cities(RenderSnapshot *snapshot) {
    int i;
    snapshot->city_count = clamp(city_count, 0, MAX_CITIES);
    for (i = 0; i < snapshot->city_count; i++) {
        SnapshotCity *dst = &snapshot->cities[i];
        City *src = &cities[i];
        dst->alive = src->alive;
        dst->owner = src->owner;
        dst->x = src->x;
        dst->y = src->y;
        dst->population = src->population;
        dst->radius = src->radius;
        dst->capital = src->capital;
        dst->port = src->port;
        dst->port_x = src->port_x;
        dst->port_y = src->port_y;
        snprintf(dst->name, sizeof(dst->name), "%s", src->name);
    }
}

static void copy_regions(RenderSnapshot *snapshot) {
    int i;
    snapshot->region_count = clamp(region_count, 0, MAX_NATURAL_REGIONS);
    for (i = 0; i < snapshot->region_count; i++) {
        SnapshotRegion *dst = &snapshot->regions[i];
        NaturalRegion *src = &natural_regions[i];
        dst->alive = src->alive;
        dst->owner = src->owner_civ;
        dst->center_x = src->center_x;
        dst->center_y = src->center_y;
        dst->tile_count = src->tile_count;
        dst->city_id = src->city_id;
        dst->name_id = src->name_id;
        snprintf(dst->name_en, sizeof(dst->name_en), "%s", province_display_name(i, 0));
        snprintf(dst->name_zh, sizeof(dst->name_zh), "%s", province_display_name(i, 1));
    }
}

static void copy_lanes(RenderSnapshot *snapshot) {
    const SeaLane *lanes;
    int count;
    int i;
    lanes = sea_lanes_get(&count);
    snapshot->lane_count = clamp(count, 0, MAX_SEA_LANES);
    for (i = 0; i < snapshot->lane_count; i++) {
        SnapshotSeaLane *dst = &snapshot->lanes[i];
        const SeaLane *src = &lanes[i];
        dst->active = src->active;
        dst->type = src->type;
        dst->from_node = src->from_node;
        dst->to_node = src->to_node;
        dst->from_region = src->from_region;
        dst->to_region = src->to_region;
        dst->from_city = src->from_city;
        dst->to_city = src->to_city;
        dst->from_port = src->from_port;
        dst->to_port = src->to_port;
        dst->from_sea_entry = src->from_sea_entry;
        dst->to_sea_entry = src->to_sea_entry;
        dst->point_count = clamp(src->point_count, 0, MAX_SEA_LANE_POINTS);
        memcpy(dst->points, src->points, (size_t)dst->point_count * sizeof(dst->points[0]));
        dst->exposure = src->exposure;
        snapshot->plague_lane_exposure[i] = src->exposure;
    }
}

static void copy_plague(RenderSnapshot *snapshot) {
    int i;
    snapshot->plague_active = 0;
    for (i = 0; i < snapshot->city_count; i++) {
        int severity = plague_city_severity(i);
        snapshot->plague_city_severity[i] = severity;
        if (severity > 0) snapshot->plague_active = 1;
    }
}

static void copy_events(RenderSnapshot *snapshot) {
    int max_events = min(event_log_count, RENDER_SNAPSHOT_EVENT_COUNT);
    int i;
    snapshot->event_count = max_events;
    snapshot->event_total_entries = event_log_total_entries;
    for (i = 0; i < max_events; i++) {
        SnapshotEvent *dst = &snapshot->events[i];
        memset(dst, 0, sizeof(*dst));
        event_log_get_entry(i, &dst->entry);
        dst->type = dst->entry.type;
        event_log_format_entry_data(&dst->entry, 0, dst->text_en, sizeof(dst->text_en));
        event_log_format_entry_data(&dst->entry, 1, dst->text_zh, sizeof(dst->text_zh));
    }
}

static int combined_key(int a, int b) {
    return (a * 1000003) ^ b;
}

static int tile_revision_key(void) {
    int key = combined_key(dirty_revision_terrain(), dirty_revision_coast());
    key = combined_key(key, dirty_revision_ownership());
    key = combined_key(key, dirty_revision_province());
    key = combined_key(key, map_w * 4099 + map_h);
    key = combined_key(key, world_generated);
    return key;
}

static int lanes_revision_key(void) {
    int key = combined_key(dirty_revision_route(), dirty_revision_ownership());
    int i;
    key = combined_key(key, dirty_revision_coast());
    key = combined_key(key, maritime_route_revision());
    key = combined_key(key, maritime_ownership_revision());
    key = combined_key(key, city_count * 7 + civ_count);
    for (i = 0; i < civ_count; i++) {
        key = key * 33 + (civs[i].alive ? 1 : 0) + clamp(civs[i].tech_stage, 0, 10) * 3;
    }
    for (i = 0; i < city_count; i++) {
        if (!cities[i].alive) continue;
        key = key * 33 + cities[i].owner * 5 + cities[i].port_region + (cities[i].port ? 11 : 0);
    }
    return key;
}

void render_snapshot_init(void) {
    memset(buffers, 0, sizeof(buffers));
    memset((void *)refs, 0, sizeof(refs));
    front_index = 0;
    published_revision = 0;
    last_publish_tick = 0;
    last_publish_ms = 0;
    skipped_publish_count = 0;
    throttled_publish_count = 0;
    last_skip_reason = SNAPSHOT_SKIP_NONE;
    initialized = 1;
}

void render_snapshot_shutdown(void) {
    initialized = 0;
}

int render_snapshot_publish_from_live_state_throttled(int force) {
    RenderSnapshot *snapshot;
    int back;
    int front;
    int tile_key;
    int lane_key;
    int plague_key;
    int event_key;
    DWORD start = GetTickCount();
    if (!initialized) render_snapshot_init();
    if (!force && last_publish_tick > 0 &&
        (int)(start - (DWORD)last_publish_tick) < SNAPSHOT_MIN_INTERVAL_MS) {
        InterlockedIncrement(&skipped_publish_count);
        InterlockedIncrement(&throttled_publish_count);
        last_skip_reason = SNAPSHOT_SKIP_THROTTLED;
        return 0;
    }
    back = choose_back_buffer();
    if (back < 0) {
        InterlockedIncrement(&skipped_publish_count);
        last_skip_reason = SNAPSHOT_SKIP_NO_BACK_BUFFER;
        return 0;
    }
    front = (int)front_index;
    snapshot = &buffers[back];
    if (published_revision > 0) memcpy(snapshot, &buffers[front], sizeof(*snapshot));
    else memset(snapshot, 0, sizeof(*snapshot));
    state_read_lock();
    snapshot->map_w = clamp(map_w, 1, MAX_MAP_W);
    snapshot->map_h = clamp(map_h, 1, MAX_MAP_H);
    snapshot->year = year;
    snapshot->month = month;
    snapshot->world_generated = world_generated;
    tile_key = tile_revision_key();
    lane_key = lanes_revision_key();
    plague_key = dirty_revision_plague();
    event_key = event_log_total_entries;
    if (snapshot->revision == 0 || snapshot->tiles_revision != tile_key) {
        copy_tiles(snapshot);
        snapshot->tiles_revision = tile_key;
    }
    copy_civs(snapshot);
    copy_cities(snapshot);
    copy_regions(snapshot);
    if (world_generated && (snapshot->revision == 0 || snapshot->lanes_revision != lane_key)) {
        copy_lanes(snapshot);
        snapshot->lanes_revision = lane_key;
    } else if (!world_generated) {
        snapshot->lane_count = 0;
    }
    if (snapshot->revision == 0 || snapshot->plague_revision != plague_key || snapshot->plague_active) {
        copy_plague(snapshot);
        snapshot->plague_revision = plague_key;
    }
    if (snapshot->revision == 0 || snapshot->events_revision != event_key) {
        copy_events(snapshot);
        snapshot->events_revision = event_key;
    }
    state_read_unlock();
    snapshot->revision = (unsigned int)InterlockedIncrement(&published_revision);
    last_publish_tick = (LONG)GetTickCount();
    last_publish_ms = (LONG)(last_publish_tick - start);
    last_skip_reason = SNAPSHOT_SKIP_NONE;
    InterlockedExchange(&front_index, back);
    return 1;
}

void render_snapshot_publish_from_live_state(void) {
    render_snapshot_publish_from_live_state_throttled(1);
}

const RenderSnapshot *render_snapshot_acquire(void) {
    int idx;
    if (!initialized) render_snapshot_init();
    do {
        idx = (int)front_index;
        InterlockedIncrement(&refs[idx]);
        if (idx == (int)front_index) return &buffers[idx];
        InterlockedDecrement(&refs[idx]);
    } while (1);
}

void render_snapshot_release(const RenderSnapshot *snapshot) {
    int i;
    if (!snapshot) return;
    for (i = 0; i < 3; i++) {
        if (snapshot == &buffers[i]) {
            InterlockedDecrement(&refs[i]);
            return;
        }
    }
}

unsigned int render_snapshot_revision(void) {
    return (unsigned int)published_revision;
}

int render_snapshot_age_ms(void) {
    LONG tick = last_publish_tick;
    if (tick <= 0) return 0;
    return clamp((int)(GetTickCount() - (DWORD)tick), 0, 600000);
}

int render_snapshot_last_publish_ms(void) {
    return (int)last_publish_ms;
}

int render_snapshot_skipped_publish_count(void) {
    return (int)skipped_publish_count;
}

int render_snapshot_throttled_publish_count(void) {
    return (int)throttled_publish_count;
}

const char *render_snapshot_last_skip_reason(void) {
    switch ((int)last_skip_reason) {
        case SNAPSHOT_SKIP_THROTTLED: return "throttled";
        case SNAPSHOT_SKIP_NO_BACK_BUFFER: return "no free back buffer";
        default: return "none";
    }
}

const SnapshotTile *render_snapshot_tile_at(const RenderSnapshot *snapshot, int x, int y) {
    if (!snapshot || x < 0 || y < 0 || x >= snapshot->map_w || y >= snapshot->map_h) return NULL;
    return &snapshot->tiles[y * snapshot->map_w + x];
}

const char *render_snapshot_event_text(const RenderSnapshot *snapshot, int index, int language) {
    static const char empty[] = "";
    if (!snapshot || index < 0 || index >= snapshot->event_count) return empty;
    return language ? snapshot->events[index].text_zh : snapshot->events[index].text_en;
}

int render_snapshot_event_get_entry(const RenderSnapshot *snapshot, int index, EventLogEntry *out) {
    if (!snapshot || !out || index < 0 || index >= snapshot->event_count) return 0;
    *out = snapshot->events[index].entry;
    return 1;
}

EventLogType render_snapshot_event_get_type(const RenderSnapshot *snapshot, int index) {
    if (!snapshot || index < 0 || index >= snapshot->event_count) return EVENT_TYPE_GENERIC;
    return snapshot->events[index].type;
}
