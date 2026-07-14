#include "game/game_plague_probe_internal.h"

#include "core/event_log_store.h"
#include "core/game_types.h"
#include "core/plague_event_types.h"
#include "core/world_announcement_store.h"
#include "io/map_save.h"
#include "io/map_save_plague.h"
#include "io/map_save_state.h"
#include "sim/plague_state.h"
#include "sim/world_announcement.h"
#include "sim/world_announcement_plague.h"

#include <stdio.h>
#include <string.h>

static void setup_identity_fixture(void) {
    memset(cities, 0, sizeof(cities));
    memset(civs, 0, sizeof(civs));
    civ_count = 1;
    city_count = 1;
    civs[0].alive = 1;
    civs[0].uid = 9901;
    civs[0].symbol = 'Q';
    civs[0].color = COLOR32_RGB(44, 122, 88);
    snprintf(civs[0].name, sizeof(civs[0].name), "River Realm");
    cities[0].alive = 1;
    cities[0].owner = 0;
    cities[0].population = 50000;
    snprintf(cities[0].name, sizeof(cities[0].name), "Green River City");
}

static void fill_episode(PlagueEpisodeState *episode, int episode_id) {
    memset(episode, 0, sizeof(*episode));
    episode->active = 1;
    episode->episode_id = episode_id;
    episode->size = PLAGUE_SIZE_MEDIUM;
    episode->severity = 7;
    episode->name_id = 0;
    episode->name_cycle = 1;
    episode->origin_city_id = 0;
    episode->origin_civ_id = 0;
    episode->origin_civ_uid = civs[0].uid;
    episode->origin_civ_symbol = civs[0].symbol;
    episode->origin_civ_color = civs[0].color;
    episode->start_month = 240;
    episode->frozen_occupied_cities = 1;
    episode->spores_initial = 1;
    episode->spores_remaining = 1;
    snprintf(episode->origin_city_name, sizeof(episode->origin_city_name), "%s",
             cities[0].name);
    snprintf(episode->origin_civ_name_en, sizeof(episode->origin_civ_name_en),
             "River Realm");
    snprintf(episode->origin_civ_name_zh, sizeof(episode->origin_civ_name_zh),
             "青河国");
}

static void check_save_roundtrip(PlagueProbeContext *context) {
    PlagueModelState before;
    PlagueModelState after;
    PlagueEpisodeState episode;
    FILE *file;
    int write_ok = 0;
    int read_ok = 0;
    setup_identity_fixture();
    plague_state_reset();
    fill_episode(&episode, 41);
    plague_state_begin_episode(&episode);
    plague_state_infect_city(0, 0, 240, 42);
    plague_state_record_start(240);
    plague_state_choose_unused_name(0, NULL, NULL);
    plague_state_record_deaths(240, 0, 0, 17);
    plague_state_copy(&before);
    file = tmpfile();
    if (file) {
        write_ok = map_save_plague_write(file);
        plague_state_reset();
        rewind(file);
        read_ok = map_save_plague_read(file);
        fclose(file);
    }
    plague_state_copy(&after);
    plague_probe_check(context, "save", "v19_plague_state_roundtrip",
        write_ok && read_ok && memcmp(&before, &after, sizeof(before)) == 0,
        "write=%d read=%d bytes=%u", write_ok, read_ok, (unsigned)sizeof(before));
}

static void check_old_save_rejection(PlagueProbeContext *context) {
    FILE *file = tmpfile();
    int dynamic_result = file ? map_save_read_dynamic_state(file, 18) : 0;
    if (file) fclose(file);
    plague_probe_check(context, "save", "old_version_rejected_before_payload",
        map_save_current_version() == 19 && map_save_version_supported(19) &&
        !map_save_version_supported(18) && dynamic_result == -1,
        "current=%d v18_supported=%d dynamic_result=%d",
        map_save_current_version(), map_save_version_supported(18), dynamic_result);
}

static void check_fog_header_roundtrip(PlagueProbeContext *context) {
    int ok = map_save_probe_fog_header_roundtrip(0) &&
             map_save_probe_fog_header_roundtrip(50) &&
             map_save_probe_fog_header_roundtrip(100);
    plague_probe_check(context, "save", "v19_fog_values_roundtrip",
        ok, "values=0/50/100 retained without default override");
}

static void check_announcement_dedupe(PlagueProbeContext *context) {
    PlagueEpisodeState episode;
    PlagueEpisodeHistory history;
    int start_first;
    int start_second;
    int end_first;
    int end_second;
    setup_identity_fixture();
    plague_state_reset();
    event_log_clear();
    world_announcement_state_reset();
    fill_episode(&episode, 77);
    plague_state_begin_episode(&episode);
    plague_state_infect_city(0, 0, 240, 60);
    plague_state_note_current_country(0);
    plague_state_note_ever_country(0);
    start_first = world_announcement_plague_emit_start(&plague_state_mutable()->episode);
    start_second = world_announcement_plague_emit_start(&plague_state_mutable()->episode);
    plague_state_finish_episode(300, &history);
    end_first = world_announcement_plague_emit_end(&plague_state_mutable()->episode, &history);
    end_second = world_announcement_plague_emit_end(&plague_state_mutable()->episode, &history);
    plague_probe_check(context, "events", "start_end_stable_id_dedupe",
        start_first && !start_second && end_first && !end_second &&
        world_announcement_store_count() == 2 && event_log_store_count() == 2 &&
        plague_event_stable_id(77, PLAGUE_EVENT_START) == 154 &&
        plague_event_stable_id(77, PLAGUE_EVENT_END) == 155,
        "start=%d/%d end=%d/%d banners=%d events=%d",
        start_first, start_second, end_first, end_second,
        world_announcement_store_count(), event_log_store_count());
}

void plague_probe_run_integration(PlagueProbeContext *context) {
    check_save_roundtrip(context);
    check_old_save_rejection(context);
    check_fog_header_roundtrip(context);
    check_announcement_dedupe(context);
}
