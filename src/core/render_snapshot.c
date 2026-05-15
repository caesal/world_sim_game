#include "core/render_snapshot.h"

#include "core/dirty_flags.h"
#include "core/render_snapshot_keys.h"
#include "core/state_lock.h"
#include "data/province_names.h"
#include "sim/collapse.h"
#include "sim/civilization_slots.h"
#include "sim/decision_snapshot.h"
#include "sim/diplomacy.h"
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
        dst->current_soldiers = war_current_soldiers_for_civ(i);
        dst->aggression = src->aggression;
        dst->expansion = src->expansion;
        dst->defense = src->defense;
        dst->culture = src->culture;
        dst->governance = src->governance;
        dst->cohesion = src->cohesion;
        dst->production = src->production;
        dst->military = src->military;
        dst->commerce = src->commerce;
        dst->logistics = src->logistics;
        dst->innovation = src->innovation;
        dst->adaptation = src->adaptation;
        dst->tech_stage = src->tech_stage;
        dst->tech_progress = src->tech_progress;
        dst->tech_stage_progress_percent = technology_stage_progress_percent(i);
        dst->tech_months_to_next = technology_months_to_next(i);
        dst->tech_required_months = technology_required_months_for_civ(i);
        dst->tech_expansion_percent = technology_expansion_percent(i);
        dst->tech_resource_percent = technology_resource_percent(i);
        dst->tech_progress_percent = technology_progress_percent(i);
        dst->disorder = src->disorder;
        dst->disorder_resource = src->disorder_resource;
        dst->disorder_plague = src->disorder_plague;
        dst->disorder_migration = src->disorder_migration;
        dst->disorder_stability = src->disorder_stability;
        dst->disorder_last_pressure = src->disorder_last_pressure;
        dst->disorder_last_recovery = src->disorder_last_recovery;
        dst->disorder_last_net = src->disorder_last_net;
        dst->disorder_last_pressure_x10 = src->disorder_last_pressure_x10;
        dst->disorder_last_recovery_x10 = src->disorder_last_recovery_x10;
        dst->disorder_last_net_x10 = src->disorder_last_net_x10;
        dst->disorder_last_base_recovery_x10 = src->disorder_last_base_recovery_x10;
        dst->disorder_last_governance_recovery_x10 = src->disorder_last_governance_recovery_x10;
        dst->disorder_last_cohesion_recovery_x10 = src->disorder_last_cohesion_recovery_x10;
        dst->disorder_last_peace_recovery_x10 = src->disorder_last_peace_recovery_x10;
        dst->disorder_last_condition_recovery_x10 = src->disorder_last_condition_recovery_x10;
        dst->disorder_last_plague_decay = src->disorder_last_plague_decay;
        dst->disorder_last_war_decay = src->disorder_last_war_decay;
        dst->disorder_last_migration_decay = src->disorder_last_migration_decay;
        dst->collapse_grace_months = src->collapse_grace_months;
        dst->plague_random_immunity_months = src->plague_random_immunity_months;
        dst->plague_active_count = plague_civ_active_count(i);
        dst->plague_months_left = plague_civ_months_left(i);
        dst->plague_peak_severity = plague_civ_peak_severity(i);
        dst->plague_deaths_total = plague_civ_deaths_total(i);
        dst->war_active = war_active_for_civ(i);
        dst->war_deployed_soldiers = war_deployed_soldiers_for_civ(i);
        dst->war_available_reserve = war_available_reserve_for_civ(i);
        dst->war_front_count = war_front_count_for_civ(i);
        dst->vassal_governance_disorder = vassal_governance_disorder(i);
        dst->vassal_callable_soldiers = vassal_callable_soldiers(i);
        dst->vassal_resource_tribute = vassal_estimated_resource_tribute_from(i);
        dst->collapse_can_trigger = collapse_can_trigger(i);
        dst->collapse_block_reason = collapse_block_reason(i);
        snprintf(dst->collapse_last_reason, sizeof(dst->collapse_last_reason), "%s", collapse_last_reason(i));
        dst->capital_city = src->capital_city;
        dst->overlord = vassal_overlord(i);
        dst->vassal_count = vassal_direct_count(i);
        dst->name_id = src->name_id;
        dst->summary = summarize_country(i);
        dst->population_summary = population_country_summary(i);
        {
            DecisionSnapshot decision;
            decision_snapshot_for_civ(i, &decision);
            dst->decision = decision;
            dst->decision_expansion_weight = decision.expansion_weight; dst->decision_war_weight = decision.war_weight;
            dst->decision_stability_weight = decision.stability_weight;
            dst->decision_next_expansion_months = decision.next_expansion_months;
            snprintf(dst->main_intent, sizeof(dst->main_intent), "%s", decision.main_intent ? decision.main_intent : "");
            snprintf(dst->decision_expansion_reason, sizeof(dst->decision_expansion_reason), "%s", decision.expansion_reason ? decision.expansion_reason : "");
            snprintf(dst->decision_war_reason, sizeof(dst->decision_war_reason), "%s", decision.war_reason ? decision.war_reason : "");
            dst->decision.main_intent = dst->main_intent; dst->decision.expansion_reason = dst->decision_expansion_reason;
            dst->decision.war_reason = dst->decision_war_reason;
        }
        snprintf(dst->name_en, sizeof(dst->name_en), "%s",
                 civilization_display_name_for_language(i, 0));
        snprintf(dst->name_zh, sizeof(dst->name_zh), "%s",
                 civilization_display_name_for_language(i, 1));
    }
}

static void copy_diplomacy(RenderSnapshot *snapshot) {
    int a, b;
    for (a = 0; a < snapshot->civ_count; a++) {
        for (b = 0; b < snapshot->civ_count; b++) {
            DiplomacyRelation rel = diplomacy_relation(a, b);
            ActiveWar war = war_state_between(a, b);
            SnapshotDiplomacyRelation *dst = &snapshot->relations[a][b];
            SnapshotWar *w = &snapshot->wars[a][b];
            dst->state = rel.state; dst->relation_score = rel.relation_score;
            dst->border_tension = rel.border_tension; dst->trade_fit = rel.trade_fit;
            dst->resource_conflict = rel.resource_conflict; dst->truce_years_left = rel.truce_years_left;
            dst->border_length = rel.border_length; dst->natural_barrier = rel.natural_barrier;
            dst->years_known = rel.years_known; dst->vassal_years = rel.vassal_years;
            dst->easing_years = rel.easing_years; dst->contact_kind = rel.contact_kind;
            dst->years_distant_known = rel.years_distant_known; dst->overlord = rel.overlord; dst->vassal = rel.vassal;
            w->active = war.active; w->attacker = war.attacker; w->defender = war.defender;
            w->soldiers_a = war.soldiers_a; w->soldiers_b = war.soldiers_b;
            w->casualties_a = war.casualties_a; w->casualties_b = war.casualties_b;
            w->support_casualties_a = war.support_casualties_a; w->support_casualties_b = war.support_casualties_b;
            w->wins_a = war.wins_a; w->wins_b = war.wins_b; w->years = war.years;
            snapshot->war_front_flags[a][b] = war_front_flags(a, b);
            snapshot->war_peace_pressure[a][b] = war_peace_pressure_between(a, b);
        }
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
        dst->port_region = src->port_region;
        dst->plague_active = plague_city_active(i);
        dst->plague_severity = plague_city_severity(i);
        dst->plague_months_left = plague_city_months_left(i);
        dst->plague_deaths_total = plague_city_deaths_total(i);
        dst->region_summary = summarize_city_region(i);
        dst->population_summary = population_city_summary(i);
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
    for (i = 0; i < snapshot->lane_count; i++) {
        int exposure = sea_lanes_exposure(i);
        snapshot->plague_lane_exposure[i] = exposure;
        snapshot->lanes[i].exposure = exposure;
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
    int civ_key;
    int city_key;
    int region_key;
    int diplomacy_key;
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
    snapshot->civ_alive_count = civilization_alive_count();
    snapshot->civ_reusable_slot_count = civilization_reusable_slot_count();
    tile_key = render_snapshot_tile_revision_key();
    civ_key = render_snapshot_civs_revision_key();
    city_key = render_snapshot_cities_revision_key();
    region_key = render_snapshot_regions_revision_key();
    diplomacy_key = render_snapshot_diplomacy_revision_key();
    lane_key = render_snapshot_lanes_revision_key();
    plague_key = render_snapshot_plague_revision_key(lane_key);
    event_key = event_log_total_entries;
    snapshot->sections_copied_mask = 0;
    snapshot->sections_skipped_mask = 0;
    if (snapshot->revision == 0 || snapshot->tiles_revision != tile_key) {
        copy_tiles(snapshot);
        snapshot->tiles_revision = tile_key;
        snapshot->sections_copied_mask |= RENDER_SNAPSHOT_SECTION_TILES;
    } else snapshot->sections_skipped_mask |= RENDER_SNAPSHOT_SECTION_TILES;
    if (snapshot->revision == 0 || snapshot->civs_revision != civ_key) {
        copy_civs(snapshot);
        snapshot->civs_revision = civ_key;
        snapshot->sections_copied_mask |= RENDER_SNAPSHOT_SECTION_CIVS;
    } else snapshot->sections_skipped_mask |= RENDER_SNAPSHOT_SECTION_CIVS;
    if (snapshot->revision == 0 || snapshot->cities_revision != city_key) {
        copy_cities(snapshot);
        snapshot->cities_revision = city_key;
        snapshot->sections_copied_mask |= RENDER_SNAPSHOT_SECTION_CITIES;
    } else snapshot->sections_skipped_mask |= RENDER_SNAPSHOT_SECTION_CITIES;
    if (snapshot->revision == 0 || snapshot->regions_revision != region_key) {
        copy_regions(snapshot);
        snapshot->regions_revision = region_key;
        snapshot->sections_copied_mask |= RENDER_SNAPSHOT_SECTION_REGIONS;
    } else snapshot->sections_skipped_mask |= RENDER_SNAPSHOT_SECTION_REGIONS;
    if (snapshot->revision == 0 || snapshot->diplomacy_revision != diplomacy_key) {
        copy_diplomacy(snapshot);
        snapshot->diplomacy_revision = diplomacy_key;
        snapshot->sections_copied_mask |= RENDER_SNAPSHOT_SECTION_DIPLOMACY;
    } else snapshot->sections_skipped_mask |= RENDER_SNAPSHOT_SECTION_DIPLOMACY;
    if (world_generated && (snapshot->revision == 0 || snapshot->lanes_revision != lane_key)) {
        copy_lanes(snapshot);
        snapshot->lanes_revision = lane_key;
        snapshot->sections_copied_mask |= RENDER_SNAPSHOT_SECTION_LANES;
    } else if (!world_generated) {
        snapshot->lane_count = 0;
        snapshot->sections_skipped_mask |= RENDER_SNAPSHOT_SECTION_LANES;
    } else snapshot->sections_skipped_mask |= RENDER_SNAPSHOT_SECTION_LANES;
    if (snapshot->revision == 0 || snapshot->plague_revision != plague_key) {
        copy_plague(snapshot);
        snapshot->plague_revision = plague_key;
        snapshot->sections_copied_mask |= RENDER_SNAPSHOT_SECTION_PLAGUE;
    } else snapshot->sections_skipped_mask |= RENDER_SNAPSHOT_SECTION_PLAGUE;
    if (snapshot->revision == 0 || snapshot->events_revision != event_key) {
        copy_events(snapshot);
        snapshot->events_revision = event_key;
        snapshot->sections_copied_mask |= RENDER_SNAPSHOT_SECTION_EVENTS;
    } else snapshot->sections_skipped_mask |= RENDER_SNAPSHOT_SECTION_EVENTS;
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
