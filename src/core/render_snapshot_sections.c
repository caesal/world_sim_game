#include "core/render_snapshot_sections.h"

#include <string.h>

static void bind_civ_decision_strings(RenderSnapshot *snapshot) {
    int i;
    if (!snapshot) return;
    for (i = 0; i < MAX_CIVS; i++) {
        SnapshotCiv *civ = &snapshot->civs[i];
        civ->decision.main_intent = civ->main_intent;
        civ->decision.expansion_reason = civ->decision_expansion_reason;
        civ->decision.war_reason = civ->decision_war_reason;
    }
}

void render_snapshot_copy_skipped_sections(RenderSnapshot *dst, const RenderSnapshot *src) {
    int mask;
    if (!dst || !src) return;
    mask = dst->sections_skipped_mask;
    if (mask & RENDER_SNAPSHOT_SECTION_TILES) {
        memcpy(dst->tiles, src->tiles, sizeof(dst->tiles));
        dst->tiles_revision = src->tiles_revision;
        dst->terrain_revision = src->terrain_revision;
        dst->coast_revision = src->coast_revision;
        dst->hydrology_revision = src->hydrology_revision;
    }
    if (mask & RENDER_SNAPSHOT_SECTION_CIVS) {
        memcpy(dst->civs, src->civs, sizeof(dst->civs));
        dst->civ_count = src->civ_count;
        dst->civ_independent_alive_count = src->civ_independent_alive_count;
        dst->civs_revision = src->civs_revision;
        bind_civ_decision_strings(dst);
    }
    if (mask & RENDER_SNAPSHOT_SECTION_CITIES) {
        memcpy(dst->cities, src->cities, sizeof(dst->cities));
        dst->city_count = src->city_count;
        dst->cities_revision = src->cities_revision;
        dst->city_visual_revision = src->city_visual_revision;
    }
    if (mask & RENDER_SNAPSHOT_SECTION_REGIONS) {
        memcpy(dst->regions, src->regions, sizeof(dst->regions));
        dst->region_count = src->region_count;
        dst->regions_revision = src->regions_revision;
    }
    if (mask & RENDER_SNAPSHOT_SECTION_DIPLOMACY) {
        memcpy(dst->relations, src->relations, sizeof(dst->relations));
        memcpy(dst->wars, src->wars, sizeof(dst->wars));
        memcpy(dst->war_front_flags, src->war_front_flags, sizeof(dst->war_front_flags));
        memcpy(dst->war_peace_pressure, src->war_peace_pressure, sizeof(dst->war_peace_pressure));
        dst->diplomacy_revision = src->diplomacy_revision;
    }
    if (mask & RENDER_SNAPSHOT_SECTION_LANES) {
        memcpy(dst->lanes, src->lanes, sizeof(dst->lanes));
        dst->lane_count = src->lane_count;
        dst->lanes_revision = src->lanes_revision;
    }
    if (mask & RENDER_SNAPSHOT_SECTION_PLAGUE) {
        memcpy(dst->plague_city_severity, src->plague_city_severity, sizeof(dst->plague_city_severity));
        memcpy(dst->plague_lane_exposure, src->plague_lane_exposure, sizeof(dst->plague_lane_exposure));
        dst->plague_active = src->plague_active;
        dst->plague_revision = src->plague_revision;
    }
    if (mask & RENDER_SNAPSHOT_SECTION_EVENTS) {
        memcpy(dst->events, src->events, sizeof(dst->events));
        memcpy(dst->civ_recent_events, src->civ_recent_events, sizeof(dst->civ_recent_events));
        memcpy(dst->civ_recent_event_count, src->civ_recent_event_count, sizeof(dst->civ_recent_event_count));
        dst->event_count = src->event_count;
        dst->event_total_entries = src->event_total_entries;
        dst->events_revision = src->events_revision;
    }
}

void render_snapshot_seed_from_front(RenderSnapshot *dst, const RenderSnapshot *src) {
    int saved_mask;
    if (!dst || !src) return;
    dst->map_w = src->map_w;
    dst->map_h = src->map_h;
    dst->year = src->year;
    dst->month = src->month;
    dst->world_generated = src->world_generated;
    dst->terrain_revision = src->terrain_revision;
    dst->coast_revision = src->coast_revision;
    dst->hydrology_revision = src->hydrology_revision;
    dst->civ_alive_count = src->civ_alive_count;
    dst->civ_independent_alive_count = src->civ_independent_alive_count;
    dst->civ_reusable_slot_count = src->civ_reusable_slot_count;
    dst->revision = src->revision;
    saved_mask = dst->sections_skipped_mask;
    dst->sections_skipped_mask =
        RENDER_SNAPSHOT_SECTION_TILES | RENDER_SNAPSHOT_SECTION_CIVS |
        RENDER_SNAPSHOT_SECTION_CITIES | RENDER_SNAPSHOT_SECTION_REGIONS |
        RENDER_SNAPSHOT_SECTION_DIPLOMACY | RENDER_SNAPSHOT_SECTION_LANES |
        RENDER_SNAPSHOT_SECTION_PLAGUE | RENDER_SNAPSHOT_SECTION_EVENTS;
    render_snapshot_copy_skipped_sections(dst, src);
    dst->sections_skipped_mask = saved_mask;
}
