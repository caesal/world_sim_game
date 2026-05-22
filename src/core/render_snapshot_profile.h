#ifndef WORLD_SIM_RENDER_SNAPSHOT_PROFILE_H
#define WORLD_SIM_RENDER_SNAPSHOT_PROFILE_H

typedef enum {
    SNAPSHOT_PROFILE_TILES,
    SNAPSHOT_PROFILE_CIVS,
    SNAPSHOT_PROFILE_CITIES,
    SNAPSHOT_PROFILE_REGIONS,
    SNAPSHOT_PROFILE_DIPLOMACY,
    SNAPSHOT_PROFILE_LANES,
    SNAPSHOT_PROFILE_PLAGUE,
    SNAPSHOT_PROFILE_EVENTS,
    SNAPSHOT_PROFILE_COUNT
} RenderSnapshotProfileSection;

void render_snapshot_profile_reset_sections(void);
void render_snapshot_profile_record_section(RenderSnapshotProfileSection section, int ms, int copied);
void render_snapshot_profile_record_publish(int total_ms, int lock_wait_ms, int lock_held_ms,
                                            int copied_mask, int skipped_mask);
int render_snapshot_profile_total_ms(void);
int render_snapshot_profile_lock_wait_ms(void);
int render_snapshot_profile_lock_held_ms(void);
int render_snapshot_profile_section_ms(RenderSnapshotProfileSection section);
int render_snapshot_profile_section_copied(RenderSnapshotProfileSection section);
void render_snapshot_profile_format_sections(int copied, char *buffer, int buffer_size);

#endif
