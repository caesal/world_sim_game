#ifndef WORLD_SIM_GAME_PRESENTATION_WORLD_ANNOUNCEMENT_PROBE_H
#define WORLD_SIM_GAME_PRESENTATION_WORLD_ANNOUNCEMENT_PROBE_H

#include "core/world_announcement_types.h"

#include <stdio.h>

typedef struct {
    WorldAnnouncementEvent age;
    WorldAnnouncementEvent collapse;
    WorldAnnouncementEvent union_four;
    WorldAnnouncementEvent union_long;
    WorldAnnouncementEvent vassal_independence;
    WorldAnnouncementEvent military_upgrade;
    WorldAnnouncementEvent military_downgrade;
    WorldAnnouncementEvent plague_started;
    WorldAnnouncementEvent plague_ended;
    WorldAnnouncementEvent alliance_war_started;
    WorldAnnouncementEvent alliance_war_victory;
    WorldAnnouncementEvent alliance_war_truce;
    int collapse_stable;
    int union_stable;
    int union_duplicate_count;
} WorldAnnouncementProbeBundle;

int game_presentation_world_announcement_event_probe(
    FILE *summary, WorldAnnouncementProbeBundle *bundle);
int game_presentation_world_announcement_ui_probe(
    FILE *summary, const WorldAnnouncementProbeBundle *bundle);
int game_presentation_world_announcement_layout_probe(
    FILE *summary, const WorldAnnouncementProbeBundle *bundle);

#endif
