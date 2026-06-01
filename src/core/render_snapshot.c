#include "core/render_snapshot.h"
#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "core/render_snapshot_events.h"
#include "core/render_snapshot_cache.h"
#include "core/render_snapshot_civs.h"
#include "core/render_snapshot_keys.h"
#include "core/render_snapshot_profile.h"
#include "core/render_snapshot_sections.h"
#include "core/state_lock.h"
#include "data/province_names.h"
#include "sim/collapse.h"
#include "sim/civilization_slots.h"
#include "sim/decision_snapshot.h"
#include "sim/diplomacy.h"
#include "sim/disorder.h"
#include "sim/maritime.h"
#include "sim/plague.h"
#include "sim/population.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/technology.h"
#include "sim/vassal.h"
#include "sim/war.h"
#include "sim/war_front.h"
#include "world/terrain_query.h"
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
enum { SNAPSHOT_SKIP_NONE = 0, SNAPSHOT_SKIP_THROTTLED = 1, SNAPSHOT_SKIP_NO_BACK_BUFFER = 2, SNAPSHOT_SKIP_LOCK_BUSY = 3 };
#define SNAPSHOT_BASE_MIN_INTERVAL_MS 125
#define PROFILE_SECTION(section, code) do { DWORD _s = GetTickCount(); code; \
    render_snapshot_profile_record_section(section, (int)(GetTickCount() - _s), 1); } while (0)
#define PROFILE_SKIP(section) render_snapshot_profile_record_section(section, 0, 0)

static int snapshot_min_interval_ms(void) {
    return SNAPSHOT_BASE_MIN_INTERVAL_MS;
}

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
            dst->ecology = (unsigned char)src->ecology;
            dst->resource = (unsigned char)src->resource;
            dst->river = (unsigned char)src->river;
            dst->elevation = (unsigned char)clamp(src->elevation, 0, 255);
            dst->water_depth = (unsigned char)world_water_depth_at(x, y);
            dst->water_deep_percent = (unsigned char)world_water_visual_deep_percent(x, y);
            dst->moisture = (short)src->moisture;
            dst->temperature = (short)src->temperature;
            dst->owner = (short)src->owner;
            dst->region_id = (short)src->region_id;
            dst->province_id = (short)src->province_id;
        }
    }
}

static int copy_diplomacy(RenderSnapshot *snapshot, int key) {
    int a, b;
    int complete = 1;
    for (a = 0; a < snapshot->civ_count; a++) {
        for (b = 0; b < snapshot->civ_count; b++) {
            if (!render_snapshot_cache_diplomacy_pair(a, b, key, &snapshot->relations[a][b],
                                                      &snapshot->wars[a][b],
                                                      &snapshot->war_front_flags[a][b],
                                                      &snapshot->war_peace_pressure[a][b])) {
                complete = 0;
                if (snapshot->revision == 0) {
                    memset(&snapshot->relations[a][b], 0, sizeof(snapshot->relations[a][b]));
                    memset(&snapshot->wars[a][b], 0, sizeof(snapshot->wars[a][b]));
                    snapshot->war_front_flags[a][b] = 0;
                    snapshot->war_peace_pressure[a][b] = 0;
                    render_snapshot_cache_note_diplomacy_fallback();
                }
            }
        }
    }
    return complete;
}

static int same_city_snapshot(const SnapshotCity *dst, const City *src) {
    return dst && src && dst->alive == src->alive && dst->owner == src->owner &&
           dst->x == src->x && dst->y == src->y && strcmp(dst->name, src->name) == 0;
}

static int copy_cities(RenderSnapshot *snapshot, int key) {
    int i;
    int complete = 1;
    snapshot->city_count = clamp(city_count, 0, MAX_CITIES);
    for (i = 0; i < snapshot->city_count; i++) {
        SnapshotCity *dst = &snapshot->cities[i];
        City *src = &cities[i];
        int stable = same_city_snapshot(dst, src);
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
        dst->port_region = src->port_region;
        if (!render_snapshot_cache_city_summary(i, key, &dst->region_summary,
                                                &dst->population_summary)) {
            complete = 0;
            if (!stable) {
                memset(&dst->region_summary, 0, sizeof(dst->region_summary));
                memset(&dst->population_summary, 0, sizeof(dst->population_summary));
                render_snapshot_cache_note_city_fallback();
            }
        }
        snprintf(dst->name, sizeof(dst->name), "%s", src->name);
    }
    return complete;
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
        dst->capital_x = src->capital_x;
        dst->capital_y = src->capital_y;
        dst->port_x = src->port_x;
        dst->port_y = src->port_y;
        dst->has_port_site = src->has_port_site;
        dst->development_score = src->development_score;
        dst->natural_defense = src->natural_defense;
        dst->cradle_score = src->cradle_score;
        dst->dominant_geography = src->dominant_geography;
        dst->dominant_climate = src->dominant_climate;
        dst->dominant_ecology = src->dominant_ecology;
        dst->average_stats = src->average_stats;
        dst->name_id = src->name_id;
        dst->name_heritage = src->name_heritage;
        snprintf(dst->name_en, sizeof(dst->name_en), "%s", province_display_name(i, 0));
        snprintf(dst->name_zh, sizeof(dst->name_zh), "%s", province_display_name(i, 1));
    }
}

static int copy_lanes(RenderSnapshot *snapshot, int key) {
    const SnapshotSeaLane *lanes;
    int count;
    int i;
    if (!render_snapshot_cache_lanes(key, &lanes, &count)) return 0;
    snapshot->lane_count = clamp(count, 0, MAX_SEA_LANES);
    for (i = 0; i < snapshot->lane_count; i++) {
        snapshot->lanes[i] = lanes[i];
        snapshot->plague_lane_exposure[i] = lanes[i].exposure;
    }
    return 1;
}

static int copy_plague(RenderSnapshot *snapshot, int key) {
    int i;
    int complete = 1;
    int active_known = 1;
    int active = 0;
    for (i = 0; i < snapshot->civ_count; i++) {
        SnapshotCiv *civ = &snapshot->civs[i];
        if (!render_snapshot_cache_plague_civ(i, key, &civ->plague_active_count,
                                              &civ->plague_months_left,
                                              &civ->plague_peak_severity,
                                              &civ->plague_deaths_total)) {
            complete = 0;
            if (snapshot->revision == 0) {
                civ->plague_active_count = 0;
                civ->plague_months_left = 0;
                civ->plague_peak_severity = 0;
                civ->plague_deaths_total = 0;
                render_snapshot_cache_note_plague_fallback();
            }
        }
    }
    for (i = 0; i < snapshot->city_count; i++) {
        SnapshotCity *city = &snapshot->cities[i];
        int severity = 0;
        if (render_snapshot_cache_plague_city(i, key, &city->plague_active,
                                             &severity, &city->plague_months_left,
                                             &city->plague_deaths_total)) {
            city->plague_severity = severity;
            snapshot->plague_city_severity[i] = severity;
            if (severity > 0) active = 1;
        } else {
            complete = 0;
            active_known = 0;
            if (snapshot->revision == 0) {
                city->plague_active = 0;
                city->plague_severity = 0;
                city->plague_months_left = 0;
                city->plague_deaths_total = 0;
                snapshot->plague_city_severity[i] = 0;
                render_snapshot_cache_note_plague_fallback();
            }
        }
    }
    for (i = 0; i < snapshot->lane_count; i++) {
        int exposure;
        if (render_snapshot_cache_plague_lane(i, key, &exposure)) {
            snapshot->plague_lane_exposure[i] = exposure;
            snapshot->lanes[i].exposure = exposure;
        } else {
            complete = 0;
            if (snapshot->revision == 0) render_snapshot_cache_note_plague_fallback();
        }
    }
    if (active_known) snapshot->plague_active = active;
    return complete;
}

void render_snapshot_init(void) {
    memset(buffers, 0, sizeof(buffers)); memset((void *)refs, 0, sizeof(refs));
    front_index = 0; published_revision = 0; last_publish_tick = 0; last_publish_ms = 0;
    skipped_publish_count = 0; throttled_publish_count = 0; last_skip_reason = SNAPSHOT_SKIP_NONE; initialized = 1;
    render_snapshot_cache_reset();
}

void render_snapshot_shutdown(void) {
    initialized = 0;
}

int render_snapshot_publish_from_live_state_throttled(int force) {
    RenderSnapshot *snapshot;
    int back;
    int tile_key;
    int civ_key;
    int city_key;
    int city_visual_key;
    int region_key;
    int diplomacy_key;
    int lane_key;
    int plague_key;
    int event_key;
    const RenderSnapshot *base_snapshot = NULL;
    DWORD wait_start;
    DWORD lock_start;
    DWORD lock_end;
    DWORD start = GetTickCount();
    int copy_plague_after_unlock = 0;
    if (!initialized) render_snapshot_init();
    if (!force && last_publish_tick > 0 &&
        (int)(start - (DWORD)last_publish_tick) < snapshot_min_interval_ms()) {
        InterlockedIncrement(&skipped_publish_count);
        InterlockedIncrement(&throttled_publish_count);
        last_skip_reason = SNAPSHOT_SKIP_THROTTLED;
        return 0;
    }
    if (published_revision > 0) base_snapshot = render_snapshot_acquire();
    back = choose_back_buffer();
    if (back < 0) {
        if (base_snapshot) render_snapshot_release(base_snapshot);
        InterlockedIncrement(&skipped_publish_count);
        last_skip_reason = SNAPSHOT_SKIP_NO_BACK_BUFFER;
        return 0;
    }
    snapshot = &buffers[back];
    if (base_snapshot) render_snapshot_seed_from_front(snapshot, base_snapshot);
    else if (published_revision == 0 && snapshot->revision == 0) memset(snapshot, 0, sizeof(*snapshot));
    render_snapshot_profile_reset_sections();
    render_snapshot_cache_begin_snapshot_copy();
    wait_start = GetTickCount();
    if (!force && !state_try_read_lock()) {
        if (base_snapshot) render_snapshot_release(base_snapshot);
        InterlockedIncrement(&skipped_publish_count);
        last_skip_reason = SNAPSHOT_SKIP_LOCK_BUSY;
        return 0;
    }
    if (force) state_read_lock();
    lock_start = GetTickCount();
    snapshot->map_w = clamp(map_w, 1, MAX_MAP_W);
    snapshot->map_h = clamp(map_h, 1, MAX_MAP_H);
    snapshot->year = year;
    snapshot->month = month;
    snapshot->world_generated = world_generated;
    snapshot->civ_alive_count = civilization_alive_count();
    snapshot->civ_reusable_slot_count = civilization_reusable_slot_count();
    snapshot->terrain_revision = dirty_revision_terrain();
    snapshot->coast_revision = dirty_revision_coast();
    snapshot->hydrology_revision = dirty_revision_hydrology();
    tile_key = render_snapshot_tile_revision_key();
    civ_key = render_snapshot_civs_revision_key();
    city_key = render_snapshot_cities_revision_key();
    city_visual_key = render_snapshot_city_visual_revision_key();
    region_key = render_snapshot_regions_revision_key();
    diplomacy_key = render_snapshot_diplomacy_revision_key();
    lane_key = render_snapshot_lanes_revision_key();
    plague_key = render_snapshot_plague_revision_key(lane_key);
    event_key = event_log_total_entries;
    snapshot->sections_copied_mask = 0;
    snapshot->sections_skipped_mask = 0;
    if (snapshot->revision == 0 || snapshot->tiles_revision != tile_key) {
        PROFILE_SECTION(SNAPSHOT_PROFILE_TILES, copy_tiles(snapshot));
        snapshot->tiles_revision = tile_key;
        snapshot->sections_copied_mask |= RENDER_SNAPSHOT_SECTION_TILES;
    } else { PROFILE_SKIP(SNAPSHOT_PROFILE_TILES); snapshot->sections_skipped_mask |= RENDER_SNAPSHOT_SECTION_TILES; }
    if (snapshot->revision == 0 || snapshot->civs_revision != civ_key) {
        PROFILE_SECTION(SNAPSHOT_PROFILE_CIVS, render_snapshot_copy_civs_locked(snapshot));
        snapshot->civs_revision = civ_key;
        snapshot->sections_copied_mask |= RENDER_SNAPSHOT_SECTION_CIVS;
    } else { PROFILE_SKIP(SNAPSHOT_PROFILE_CIVS); snapshot->sections_skipped_mask |= RENDER_SNAPSHOT_SECTION_CIVS; }
    if (snapshot->revision == 0 || snapshot->cities_revision != city_key) {
        int complete = 0;
        PROFILE_SECTION(SNAPSHOT_PROFILE_CITIES, complete = copy_cities(snapshot, city_key));
        if (complete) snapshot->cities_revision = city_key;
        snapshot->city_visual_revision = city_visual_key;
        snapshot->sections_copied_mask |= RENDER_SNAPSHOT_SECTION_CITIES;
    } else {
        snapshot->city_visual_revision = city_visual_key;
        PROFILE_SKIP(SNAPSHOT_PROFILE_CITIES);
        snapshot->sections_skipped_mask |= RENDER_SNAPSHOT_SECTION_CITIES;
    }
    if (snapshot->revision == 0 || snapshot->regions_revision != region_key) {
        PROFILE_SECTION(SNAPSHOT_PROFILE_REGIONS, copy_regions(snapshot));
        snapshot->regions_revision = region_key;
        snapshot->sections_copied_mask |= RENDER_SNAPSHOT_SECTION_REGIONS;
    } else { PROFILE_SKIP(SNAPSHOT_PROFILE_REGIONS); snapshot->sections_skipped_mask |= RENDER_SNAPSHOT_SECTION_REGIONS; }
    if (snapshot->revision == 0 || snapshot->diplomacy_revision != diplomacy_key) {
        int complete = 0;
        if (base_snapshot && !render_snapshot_cache_diplomacy_ready(diplomacy_key)) {
            PROFILE_SKIP(SNAPSHOT_PROFILE_DIPLOMACY);
            snapshot->sections_skipped_mask |= RENDER_SNAPSHOT_SECTION_DIPLOMACY;
        } else {
            PROFILE_SECTION(SNAPSHOT_PROFILE_DIPLOMACY, complete = copy_diplomacy(snapshot, diplomacy_key));
            if (complete) snapshot->diplomacy_revision = diplomacy_key;
            snapshot->sections_copied_mask |= RENDER_SNAPSHOT_SECTION_DIPLOMACY;
        }
    } else { PROFILE_SKIP(SNAPSHOT_PROFILE_DIPLOMACY); snapshot->sections_skipped_mask |= RENDER_SNAPSHOT_SECTION_DIPLOMACY; }
    if (world_generated && (snapshot->revision == 0 || snapshot->lanes_revision != lane_key)) {
        int complete = 0;
        PROFILE_SECTION(SNAPSHOT_PROFILE_LANES, complete = copy_lanes(snapshot, lane_key));
        if (complete) snapshot->lanes_revision = lane_key;
        snapshot->sections_copied_mask |= RENDER_SNAPSHOT_SECTION_LANES;
    } else if (!world_generated) {
        snapshot->lane_count = 0;
        PROFILE_SKIP(SNAPSHOT_PROFILE_LANES);
        snapshot->sections_skipped_mask |= RENDER_SNAPSHOT_SECTION_LANES;
    } else { PROFILE_SKIP(SNAPSHOT_PROFILE_LANES); snapshot->sections_skipped_mask |= RENDER_SNAPSHOT_SECTION_LANES; }
    if (snapshot->revision == 0 || snapshot->plague_revision != plague_key) {
        int complete = 0;
        if (!force && base_snapshot) {
            copy_plague_after_unlock = 1;
            PROFILE_SKIP(SNAPSHOT_PROFILE_PLAGUE);
        } else {
            PROFILE_SECTION(SNAPSHOT_PROFILE_PLAGUE, complete = copy_plague(snapshot, plague_key));
            if (complete) snapshot->plague_revision = plague_key;
        }
        snapshot->sections_copied_mask |= RENDER_SNAPSHOT_SECTION_PLAGUE;
    } else { PROFILE_SKIP(SNAPSHOT_PROFILE_PLAGUE); snapshot->sections_skipped_mask |= RENDER_SNAPSHOT_SECTION_PLAGUE; }
    if (snapshot->revision == 0 || snapshot->events_revision != event_key) {
        PROFILE_SECTION(SNAPSHOT_PROFILE_EVENTS, render_snapshot_copy_events_locked(snapshot));
        snapshot->events_revision = event_key;
        snapshot->sections_copied_mask |= RENDER_SNAPSHOT_SECTION_EVENTS;
    } else { PROFILE_SKIP(SNAPSHOT_PROFILE_EVENTS); snapshot->sections_skipped_mask |= RENDER_SNAPSHOT_SECTION_EVENTS; }
    lock_end = GetTickCount();
    state_read_unlock();
    if (copy_plague_after_unlock) {
        int complete = 0;
        DWORD plague_start = GetTickCount();
        complete = copy_plague(snapshot, plague_key);
        render_snapshot_profile_record_section(SNAPSHOT_PROFILE_PLAGUE,
                                               (int)(GetTickCount() - plague_start), 1);
        if (complete) snapshot->plague_revision = plague_key;
    }
    if (snapshot->sections_copied_mask & RENDER_SNAPSHOT_SECTION_EVENTS) render_snapshot_format_events(snapshot);
    snapshot->revision = (unsigned int)InterlockedIncrement(&published_revision);
    last_publish_tick = (LONG)GetTickCount();
    last_publish_ms = (LONG)(last_publish_tick - start);
    render_snapshot_profile_record_publish((int)last_publish_ms, (int)(lock_start - wait_start),
                                           (int)(lock_end - lock_start),
                                           snapshot->sections_copied_mask, snapshot->sections_skipped_mask);
    last_skip_reason = SNAPSHOT_SKIP_NONE;
    InterlockedExchange(&front_index, back);
    if (base_snapshot) render_snapshot_release(base_snapshot);
    return 1;
}

void render_snapshot_publish_from_live_state(void) { render_snapshot_publish_from_live_state_throttled(1); }

const RenderSnapshot *render_snapshot_acquire(void) {
    int idx; if (!initialized) render_snapshot_init();
    do { idx = (int)front_index; InterlockedIncrement(&refs[idx]);
        if (idx == (int)front_index) return &buffers[idx];
        InterlockedDecrement(&refs[idx]); } while (1);
}

void render_snapshot_release(const RenderSnapshot *snapshot) {
    int i; if (!snapshot) return;
    for (i = 0; i < 3; i++) if (snapshot == &buffers[i]) { InterlockedDecrement(&refs[i]); return; }
}

unsigned int render_snapshot_revision(void) { return (unsigned int)published_revision; }

int render_snapshot_age_ms(void) {
    LONG tick = last_publish_tick; if (tick <= 0) return 0;
    return clamp((int)(GetTickCount() - (DWORD)tick), 0, 600000); }

int render_snapshot_last_publish_ms(void) { return (int)last_publish_ms; }
int render_snapshot_skipped_publish_count(void) { return (int)skipped_publish_count; }
int render_snapshot_throttled_publish_count(void) { return (int)throttled_publish_count; }
const char *render_snapshot_last_skip_reason(void) {
    switch ((int)last_skip_reason) {
        case SNAPSHOT_SKIP_THROTTLED: return "throttled";
        case SNAPSHOT_SKIP_NO_BACK_BUFFER: return "no free back buffer";
        case SNAPSHOT_SKIP_LOCK_BUSY: return "lock busy";
        default: return "none";
    }
}

const SnapshotTile *render_snapshot_tile_at(const RenderSnapshot *snapshot, int x, int y) {
    if (!snapshot || x < 0 || y < 0 || x >= snapshot->map_w || y >= snapshot->map_h) return NULL;
    return &snapshot->tiles[y * snapshot->map_w + x]; }

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

int render_snapshot_civ_recent_event_count(const RenderSnapshot *snapshot, int civ_id) {
    if (!snapshot || civ_id < 0 || civ_id >= MAX_CIVS) return 0;
    return clamp(snapshot->civ_recent_event_count[civ_id], 0, EVENT_LOG_CIV_HISTORY_COUNT); }

int render_snapshot_civ_recent_event_get_entry(const RenderSnapshot *snapshot, int civ_id,
                                               int index, EventLogEntry *out) {
    if (!snapshot || !out || civ_id < 0 || civ_id >= MAX_CIVS ||
        index < 0 || index >= render_snapshot_civ_recent_event_count(snapshot, civ_id)) return 0;
    *out = snapshot->civ_recent_events[civ_id][index].entry; return 1; }
