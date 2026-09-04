#include "game/game_decision_cache_probe_lifecycle.h"

#include "core/dirty_flags.h"
#include "core/render_snapshot.h"
#include "core/render_snapshot_cache.h"
#include "core/render_snapshot_keys.h"
#include "core/render_snapshot_plague_cache.h"
#include "game/game.h"
#include "game/game_decision_cache_probe_fixture.h"
#include "render/panel_country_decision.h"
#include "render/panel_view_model_cache_keys.h"
#include "sim/decision_snapshot_cache.h"
#include "sim/plague_state.h"
#include "sim/simulation.h"
#include "sim/war_history.h"
#include "ui/ui_types.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int finish_generation(void) {
    int guard = 0;
    while (!decision_snapshot_cache_service_slice() && guard++ < MAX_CIVS + 4) {}
    return guard <= MAX_CIVS + 4;
}

static int case_plague_cache_decision_publication(FILE *summary) {
    DecisionSnapshotCacheDiagnostics decision_before;
    DecisionSnapshotCacheDiagnostics decision_after;
    PlagueEpisodeHistory history;
    PlagueStateView cached_state;
    const RenderSnapshot *snapshot;
    int lane_key;
    int lane_key_after;
    int civ_key_before;
    int civ_key_after;
    int plague_key_before;
    int plague_key_after;
    int empty_front = 0;
    int cache_before = 0;
    int cache_after = 0;
    int snapshot_history = 0;
    int history_identity = 0;
    int decision_revision_changed;
    int lane_key_stable;
    int civ_key_changed;
    int plague_key_stable;
    int war_civ_key_changed;
    int war_plague_key_stable;
    int plague_changed;
    int month_changed;
    int city_changed;
    int civ_changed;
    int previous_key;
    int ok = 1;

    decision_cache_probe_setup_fixture(2, 220000);
    memset(&cities[0], 0, sizeof(cities[0]));
    cities[0].alive = 1;
    cities[0].owner = 0;
    cities[0].population = 1000;
    snprintf(cities[0].name, sizeof(cities[0].name), "Cache Probe Origin");
    city_count = 1;
    civs[0].capital_city = 0;
    decision_snapshot_cache_seed_complete();
    render_snapshot_init();
    render_snapshot_cache_update_all();
    render_snapshot_publish_from_live_state();
    snapshot = render_snapshot_acquire();
    if (snapshot) {
        empty_front = snapshot->plague_state.recent_history_count == 0;
        render_snapshot_release(snapshot);
    }

    memset(&history, 0, sizeof(history));
    history.episode_id = 901;
    history.size = PLAGUE_SIZE_MEDIUM;
    history.severity = 7;
    history.name_id = 0;
    history.name_cycle = 1;
    history.origin_city_id = 0;
    history.origin_civ_id = 0;
    history.origin_civ_uid = civs[0].uid;
    snprintf(history.origin_city_name, sizeof(history.origin_city_name),
             "Cache Probe Origin");
    snprintf(history.origin_civ_name_en, sizeof(history.origin_civ_name_en),
             "%s", civs[0].name);
    snprintf(history.origin_civ_name_zh, sizeof(history.origin_civ_name_zh),
             "%s", civs[0].name);
    history.origin_civ_symbol = civs[0].symbol;
    history.origin_civ_color = civs[0].color;
    history.start_month = 240;
    history.end_month = 376;
    history.duration_months = 136;
    history.spores_initial = 12;
    history.spores_used = 12;
    history.infected_city_count = 4;
    history.affected_country_count = 2;
    history.total_deaths = 90136;
    plague_state_push_history(&history);
    dirty_mark_plague();

    lane_key = render_snapshot_lanes_revision_key();
    civ_key_before = render_snapshot_civs_revision_key();
    plague_key_before = render_snapshot_plague_revision_key(lane_key);
    render_snapshot_plague_cache_refresh(plague_key_before);
    memset(&cached_state, 0, sizeof(cached_state));
    cache_before = !render_snapshot_plague_cache_is_dirty(plague_key_before) &&
        render_snapshot_cache_plague_summary(plague_key_before, &cached_state,
                                             NULL, NULL) &&
        cached_state.recent_history_count == 1 &&
        cached_state.recent_history[0].episode_id == history.episode_id;

    decision_snapshot_cache_get_diagnostics(&decision_before);
    decision_snapshot_cache_mark_all_dirty();
    decision_snapshot_cache_begin_generation();
    ok &= finish_generation();
    decision_snapshot_cache_get_diagnostics(&decision_after);
    lane_key_after = render_snapshot_lanes_revision_key();
    civ_key_after = render_snapshot_civs_revision_key();
    plague_key_after = render_snapshot_plague_revision_key(lane_key_after);
    decision_revision_changed =
        decision_after.published_revision == decision_before.published_revision + 1;
    lane_key_stable = lane_key_after == lane_key;
    civ_key_changed = civ_key_after != civ_key_before;
    plague_key_stable = plague_key_after == plague_key_before;
    memset(&cached_state, 0, sizeof(cached_state));
    cache_after = !render_snapshot_plague_cache_is_dirty(plague_key_after) &&
        render_snapshot_cache_plague_summary(plague_key_after, &cached_state,
                                             NULL, NULL) &&
        cached_state.recent_history_count == 1 &&
        cached_state.recent_history[0].episode_id == history.episode_id;

    render_snapshot_publish_from_live_state();
    snapshot = render_snapshot_acquire();
    if (snapshot) {
        snapshot_history = snapshot->plague_state.recent_history_count == 1;
        history_identity = snapshot_history &&
            snapshot->plague_revision == plague_key_after &&
            snapshot->plague_state.recent_history[0].episode_id == history.episode_id &&
            snapshot->plague_state.recent_history[0].start_month == history.start_month &&
            snapshot->plague_state.recent_history[0].end_month == history.end_month &&
            snapshot->plague_state.recent_history[0].duration_months ==
                history.duration_months &&
            snapshot->plague_names.history_en[0][0] != '\0' &&
            snapshot->plague_names.history_zh[0][0] != '\0';
        render_snapshot_release(snapshot);
    }

    civ_key_before = render_snapshot_civs_revision_key();
    plague_key_before = render_snapshot_plague_revision_key(lane_key);
    war_history_reset();
    war_civ_key_changed = render_snapshot_civs_revision_key() != civ_key_before;
    war_plague_key_stable =
        render_snapshot_plague_revision_key(lane_key) == plague_key_before;

    previous_key = render_snapshot_plague_revision_key(lane_key);
    history.episode_id++;
    plague_state_push_history(&history);
    dirty_mark_plague();
    plague_changed = render_snapshot_plague_revision_key(lane_key) != previous_key;

    previous_key = render_snapshot_plague_revision_key(lane_key);
    decision_cache_probe_advance_month();
    month_changed = render_snapshot_plague_revision_key(lane_key) != previous_key;

    previous_key = render_snapshot_plague_revision_key(lane_key);
    cities[0].population++;
    dirty_mark_city();
    city_changed = render_snapshot_plague_revision_key(lane_key) != previous_key;

    previous_key = render_snapshot_plague_revision_key(lane_key);
    civs[0].governance++;
    dirty_mark_civ();
    civ_changed = render_snapshot_plague_revision_key(lane_key) != previous_key;

    ok &= empty_front && cache_before && decision_revision_changed && lane_key_stable &&
          civ_key_changed && plague_key_stable && cache_after &&
          snapshot_history && history_identity && war_civ_key_changed &&
          war_plague_key_stable && plague_changed && month_changed &&
          city_changed && civ_changed;
    fprintf(summary,
            "lifecycle=plague_cache_decision_publication ok=%d empty_front=%d cache=%d/%d decision_revision=%llu/%llu lane_key_stable=%d civ_key_changed=%d plague_key_stable=%d snapshot_history=%d history_identity=%d episode=%d start=%d end=%d duration=%d war_keys=%d/%d invalidations=%d/%d/%d/%d\n",
            ok, empty_front, cache_before, cache_after,
            (unsigned long long)decision_before.published_revision,
            (unsigned long long)decision_after.published_revision,
            lane_key_stable, civ_key_changed, plague_key_stable,
            snapshot_history, history_identity,
            history.episode_id - 1, history.start_month, history.end_month,
            history.duration_months, war_civ_key_changed, war_plague_key_stable,
            plague_changed, month_changed, city_changed, civ_changed);
    render_snapshot_shutdown();
    simulation_reset_state();
    return ok;
}

static int case_pause_switch_redraw(FILE *summary) {
    DecisionSnapshotCacheDiagnostics before;
    DecisionSnapshotCacheDiagnostics partial;
    DecisionSnapshotCacheDiagnostics after;
    const RenderSnapshot *snapshot;
    uint64_t published_hash;
    uint64_t calculations;
    uint64_t paused_delta;
    unsigned int keys[4];
    int ids[4] = {0, 4, 25, 25};
    int old_selected = selected_civ;
    int old_subtab = country_decision_subtab;
    int old_language = ui_language;
    int i;
    int ok;
    decision_cache_probe_setup_fixture(26, 210000);
    decision_snapshot_cache_seed_complete();
    decision_snapshot_cache_get_diagnostics(&before);
    published_hash = decision_cache_probe_published_hash();
    decision_cache_probe_advance_month();
    decision_snapshot_cache_begin_generation();
    if (decision_snapshot_cache_service_slice()) return 0;
    decision_snapshot_cache_get_diagnostics(&partial);
    calculations = decision_snapshot_cache_total_calculation_count();
    auto_run = 0;
    render_snapshot_init();
    render_snapshot_cache_update_all();
    render_snapshot_publish_from_live_state();
    render_snapshot_publish_from_live_state();
    snapshot = render_snapshot_acquire();
    if (!snapshot) {
        keys[0] = keys[1] = keys[2] = keys[3] = 0;
    } else {
        for (i = 0; i < 4; i++) {
            selected_civ = ids[i];
            country_decision_subtab = i;
            ui_language = i % 2;
            keys[i] = panel_view_model_cache_probe_decision_key(&snapshot->civs[ids[i]]);
        }
        render_snapshot_release(snapshot);
    }
    render_snapshot_shutdown();
    paused_delta = decision_snapshot_cache_total_calculation_count() - calculations;
    ok = partial.building_active && partial.building_built_count > 0 &&
         partial.building_built_count < partial.building_expected_count &&
         partial.published_revision == before.published_revision &&
         decision_cache_probe_published_hash() == published_hash &&
         paused_delta == 0 &&
         keys[0] != 0 && keys[1] != 0 && keys[2] != 0 && keys[3] != 0;
    ok &= finish_generation();
    decision_snapshot_cache_get_diagnostics(&after);
    ok &= after.published_revision == before.published_revision + 1 &&
          after.published_count == 26;
    selected_civ = old_selected;
    country_decision_subtab = old_subtab;
    ui_language = old_language;
    fprintf(summary,
            "lifecycle=pause_switch_redraw ok=%d partial=%d/%d revision=%llu/%llu calculations_while_paused=%llu subtabs=4 rapid_ids=0/4/25 keys=%u/%u/%u/%u\n",
            ok, partial.building_built_count, partial.building_expected_count,
            (unsigned long long)before.published_revision,
            (unsigned long long)after.published_revision,
            (unsigned long long)paused_delta,
            keys[0], keys[1], keys[2], keys[3]);
    return ok;
}

static int case_identity_restart_death_reuse(FILE *summary) {
    DecisionSnapshotCacheDiagnostics before;
    DecisionSnapshotCacheDiagnostics restarted;
    DecisionSnapshotCacheDiagnostics death;
    DecisionSnapshotCacheDiagnostics reuse;
    DecisionSnapshot snapshot;
    uint64_t revision_before;
    int old_uid;
    int ok = 1;
    decision_snapshot_cache_get_diagnostics(&before);
    revision_before = before.published_revision;
    old_uid = civs[4].uid;
    decision_cache_probe_advance_month();
    decision_snapshot_cache_begin_generation();
    if (decision_snapshot_cache_service_slice()) return 0;
    civs[4].uid = old_uid + 1000;
    ok &= !decision_snapshot_cached(4, &snapshot);
    ok &= finish_generation();
    decision_snapshot_cache_get_diagnostics(&restarted);
    ok &= restarted.published_revision == revision_before + 1 &&
          restarted.restart_count >= 1 && restarted.uid_mismatch_count >= 1 &&
          decision_snapshot_cached(4, &snapshot) &&
          snapshot.published_revision == restarted.published_revision;

    civs[4].alive = 0;
    decision_snapshot_cache_mark_all_dirty();
    ok &= !decision_snapshot_cached(4, &snapshot);
    decision_cache_probe_advance_month();
    decision_snapshot_cache_begin_generation();
    ok &= finish_generation();
    decision_snapshot_cache_get_diagnostics(&death);
    ok &= death.published_expected_count == 25 && death.published_count == 25 &&
          !decision_snapshot_cached(4, &snapshot);

    civs[4].alive = 1;
    civs[4].uid = old_uid + 2000;
    decision_snapshot_cache_mark_all_dirty();
    ok &= !decision_snapshot_cached(4, &snapshot);
    decision_cache_probe_advance_month();
    decision_snapshot_cache_begin_generation();
    ok &= finish_generation();
    decision_snapshot_cache_get_diagnostics(&reuse);
    ok &= reuse.published_expected_count == 26 && reuse.published_count == 26 &&
          decision_snapshot_cached(4, &snapshot) &&
          snapshot.published_revision == reuse.published_revision;
    fprintf(summary,
            "lifecycle=identity_death_reuse ok=%d revisions=%llu/%llu/%llu/%llu restart=%d uid_mismatch=%d counts=%d/%d reused_uid=%d\n",
            ok, (unsigned long long)revision_before,
            (unsigned long long)restarted.published_revision,
            (unsigned long long)death.published_revision,
            (unsigned long long)reuse.published_revision,
            restarted.restart_count, restarted.uid_mismatch_count,
            death.published_count, reuse.published_count, civs[4].uid);
    return ok;
}

static int case_reset_load_first_generation(FILE *summary) {
    DecisionSnapshotCacheDiagnostics before;
    DecisionSnapshotCacheDiagnostics reset;
    DecisionSnapshotCacheDiagnostics loaded;
    DecisionSnapshot snapshot;
    uint64_t calculations;
    uint64_t load_calculations;
    int ok;
    decision_snapshot_cache_get_diagnostics(&before);
    calculations = decision_snapshot_cache_total_calculation_count();
    simulation_reset_state();
    decision_snapshot_cache_get_diagnostics(&reset);
    ok = reset.published_revision == before.published_revision + 1 &&
         reset.published_expected_count == 0 && reset.published_count == 0 &&
         reset.published_built_count == 0 &&
         decision_snapshot_cache_total_calculation_count() == calculations &&
         !decision_snapshot_cached(0, &snapshot);
    decision_cache_probe_setup_fixture(26, 310000);
    render_snapshot_init();
    load_calculations = decision_snapshot_cache_total_calculation_count();
    game_request_after_load_map(NULL, 1);
    decision_snapshot_cache_get_diagnostics(&loaded);
    ok &= loaded.published_revision > reset.published_revision &&
          loaded.published_expected_count == 26 && loaded.published_count == 26 &&
          loaded.published_year == year && loaded.published_month == month &&
          decision_snapshot_cache_total_calculation_count() - load_calculations == 26 &&
          decision_snapshot_cached(25, &snapshot) &&
          snapshot.published_revision == loaded.published_revision;
    render_snapshot_shutdown();
    fprintf(summary,
            "lifecycle=reset_post_load_first_generation ok=%d reset_revision=%llu empty=%d loaded_revision=%llu loaded=%d load_calculations=%llu late_id=25\n",
            ok, (unsigned long long)reset.published_revision, reset.published_count,
            (unsigned long long)loaded.published_revision, loaded.published_count,
            (unsigned long long)(decision_snapshot_cache_total_calculation_count() -
                                 load_calculations));
    return ok;
}

int game_decision_cache_probe_lifecycle(FILE *summary) {
    int ok = 1;
    ok &= case_plague_cache_decision_publication(summary);
    ok &= case_pause_switch_redraw(summary);
    ok &= case_identity_restart_death_reuse(summary);
    ok &= case_reset_load_first_generation(summary);
    return ok;
}
