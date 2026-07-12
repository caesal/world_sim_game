#include "sim/world_announcement.h"
#include "sim/world_announcement_internal.h"

#include "core/game_types.h"
#include "sim/alliance.h"

static void prepare_alliance_event(WorldAnnouncementEvent *event, int type, int priority,
                                   int alliance_id, int actor, int target) {
    world_announcement_event_init(event, type, priority);
    world_announcement_capture_civ(&event->actor, actor);
    world_announcement_capture_civ(&event->target, target);
    world_announcement_capture_alliance(&event->alliance_a, alliance_id);
    world_announcement_set_location_civ(event, actor >= 0 ? actor : target);
}

void world_announcement_emit_alliance_created(int alliance_id, int founder, int second) {
    WorldAnnouncementEvent event;
    char raw[EVENT_LOG_LEN];
    prepare_alliance_event(&event, EVENT_TYPE_ALLIANCE_CREATED,
                           WORLD_ANNOUNCEMENT_MAJOR, alliance_id, founder, second);
    world_announcement_alliance_raw_name(alliance_id, raw, sizeof(raw));
    world_announcement_publish(&event, EVENT_SEVERITY_INFO, founder, second,
                               -1, -1, alliance_id, 2, raw);
}

void world_announcement_emit_alliance_dissolved(int alliance_id, int actor) {
    WorldAnnouncementEvent event;
    char raw[EVENT_LOG_LEN];
    prepare_alliance_event(&event, EVENT_TYPE_ALLIANCE_DISSOLVED,
                           WORLD_ANNOUNCEMENT_MAJOR, alliance_id, actor, -1);
    world_announcement_alliance_raw_name(alliance_id, raw, sizeof(raw));
    world_announcement_publish(&event, EVENT_SEVERITY_WARNING, actor, -1,
                               -1, -1, alliance_id, 0, raw);
}

void world_announcement_emit_alliance_member_joined(int alliance_id, int founder, int member) {
    WorldAnnouncementEvent event;
    char raw[EVENT_LOG_LEN];
    prepare_alliance_event(&event, EVENT_TYPE_ALLIANCE_MEMBER_JOINED,
                           WORLD_ANNOUNCEMENT_NORMAL, alliance_id, founder, member);
    world_announcement_alliance_raw_name(alliance_id, raw, sizeof(raw));
    world_announcement_publish(&event, EVENT_SEVERITY_INFO, founder, member,
                               -1, -1, alliance_id, alliance_member_count(alliance_id), raw);
}

void world_announcement_emit_alliance_member_removed(int alliance_id, int member, int founder) {
    WorldAnnouncementEvent event;
    char raw[EVENT_LOG_LEN];
    prepare_alliance_event(&event, EVENT_TYPE_ALLIANCE_MEMBER_REMOVED,
                           WORLD_ANNOUNCEMENT_NORMAL, alliance_id, member, founder);
    world_announcement_alliance_raw_name(alliance_id, raw, sizeof(raw));
    world_announcement_publish(&event, EVENT_SEVERITY_INFO, member, -1,
                               -1, -1, alliance_id, alliance_member_count(alliance_id), raw);
}

void world_announcement_emit_alliance_military_changed(int alliance_id, int upgraded) {
    WorldAnnouncementEvent event;
    char raw[EVENT_LOG_LEN];
    int leader = alliance_founder(alliance_id);
    int type = upgraded ? EVENT_TYPE_ALLIANCE_MILITARY_UPGRADED :
                          EVENT_TYPE_ALLIANCE_MILITARY_DOWNGRADED;
    prepare_alliance_event(&event, type, WORLD_ANNOUNCEMENT_CRITICAL,
                           alliance_id, leader, -1);
    world_announcement_alliance_raw_name(alliance_id, raw, sizeof(raw));
    world_announcement_publish(&event, EVENT_SEVERITY_DANGER, leader, -1,
                               -1, -1, alliance_id, upgraded, raw);
}

void world_announcement_emit_union(int alliance_id, int proposer,
                                   const int *members, int member_count) {
    WorldAnnouncementEvent event;
    char raw[EVENT_LOG_LEN];
    int i;
    prepare_alliance_event(&event, EVENT_TYPE_DIPLOMACY_ALLIANCE_UNION,
                           WORLD_ANNOUNCEMENT_CRITICAL, alliance_id, proposer, -1);
    for (i = 0; members && i < member_count; i++) {
        if (members[i] != proposer) world_announcement_add_related(&event, members[i]);
    }
    world_announcement_alliance_raw_name(alliance_id, raw, sizeof(raw));
    world_announcement_publish(&event, EVENT_SEVERITY_DANGER, proposer, -1,
                               -1, -1, alliance_id, event.related_count, raw);
}
