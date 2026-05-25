#include "core/render_snapshot_profile.h"

#include "core/render_snapshot.h"

#include <stdio.h>
#include <string.h>

static int last_total_ms;
static int last_lock_wait_ms;
static int last_lock_held_ms;
static int last_section_ms[SNAPSHOT_PROFILE_COUNT];
static int last_section_copied[SNAPSHOT_PROFILE_COUNT];
static int last_civ_phase_ms[SNAPSHOT_CIV_PROFILE_COUNT];
static int last_copied_mask;
static int last_skipped_mask;

static const char *section_name(RenderSnapshotProfileSection section) {
    static const char *names[SNAPSHOT_PROFILE_COUNT] = {
        "tiles", "civs", "cities", "regions", "diplomacy", "lanes", "plague", "events"
    };
    return section >= 0 && section < SNAPSHOT_PROFILE_COUNT ? names[section] : "?";
}

static int section_mask(RenderSnapshotProfileSection section) {
    static const int masks[SNAPSHOT_PROFILE_COUNT] = {
        RENDER_SNAPSHOT_SECTION_TILES, RENDER_SNAPSHOT_SECTION_CIVS,
        RENDER_SNAPSHOT_SECTION_CITIES, RENDER_SNAPSHOT_SECTION_REGIONS,
        RENDER_SNAPSHOT_SECTION_DIPLOMACY, RENDER_SNAPSHOT_SECTION_LANES,
        RENDER_SNAPSHOT_SECTION_PLAGUE, RENDER_SNAPSHOT_SECTION_EVENTS
    };
    return section >= 0 && section < SNAPSHOT_PROFILE_COUNT ? masks[section] : 0;
}

void render_snapshot_profile_reset_sections(void) {
    memset(last_section_ms, 0, sizeof(last_section_ms));
    memset(last_section_copied, 0, sizeof(last_section_copied));
    memset(last_civ_phase_ms, 0, sizeof(last_civ_phase_ms));
}

void render_snapshot_profile_record_section(RenderSnapshotProfileSection section, int ms, int copied) {
    if (section < 0 || section >= SNAPSHOT_PROFILE_COUNT) return;
    last_section_ms[section] = ms;
    last_section_copied[section] = copied ? 1 : 0;
}

void render_snapshot_profile_record_civ_phase(RenderSnapshotCivProfilePhase phase, int ms) {
    if (phase < 0 || phase >= SNAPSHOT_CIV_PROFILE_COUNT) return;
    last_civ_phase_ms[phase] += ms;
}

void render_snapshot_profile_record_publish(int total_ms, int lock_wait_ms, int lock_held_ms,
                                            int copied_mask, int skipped_mask) {
    last_total_ms = total_ms;
    last_lock_wait_ms = lock_wait_ms;
    last_lock_held_ms = lock_held_ms;
    last_copied_mask = copied_mask;
    last_skipped_mask = skipped_mask;
}

int render_snapshot_profile_total_ms(void) { return last_total_ms; }
int render_snapshot_profile_lock_wait_ms(void) { return last_lock_wait_ms; }
int render_snapshot_profile_lock_held_ms(void) { return last_lock_held_ms; }

int render_snapshot_profile_section_ms(RenderSnapshotProfileSection section) {
    return section >= 0 && section < SNAPSHOT_PROFILE_COUNT ? last_section_ms[section] : 0;
}

int render_snapshot_profile_section_copied(RenderSnapshotProfileSection section) {
    return section >= 0 && section < SNAPSHOT_PROFILE_COUNT ? last_section_copied[section] : 0;
}

int render_snapshot_profile_civ_phase_ms(RenderSnapshotCivProfilePhase phase) {
    return phase >= 0 && phase < SNAPSHOT_CIV_PROFILE_COUNT ? last_civ_phase_ms[phase] : 0;
}

void render_snapshot_profile_format_sections(int copied, char *buffer, int buffer_size) {
    int mask = copied ? last_copied_mask : last_skipped_mask;
    int i;
    int used = 0;
    if (!buffer || buffer_size <= 0) return;
    buffer[0] = '\0';
    for (i = 0; i < SNAPSHOT_PROFILE_COUNT; i++) {
        if (!(mask & section_mask((RenderSnapshotProfileSection)i))) continue;
        used += snprintf(buffer + used, buffer_size - used, "%s%s",
                         used > 0 ? "," : "", section_name((RenderSnapshotProfileSection)i));
        if (used >= buffer_size) {
            buffer[buffer_size - 1] = '\0';
            return;
        }
    }
    if (used == 0) snprintf(buffer, buffer_size, "none");
}
