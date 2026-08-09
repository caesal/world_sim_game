#include "game/game_decision_cache_probe_lifecycle.h"

#include "core/render_snapshot.h"
#include "core/render_snapshot_cache.h"
#include "game/game.h"
#include "game/game_decision_cache_probe_fixture.h"
#include "render/panel_country_decision.h"
#include "render/panel_view_model_cache_keys.h"
#include "sim/decision_snapshot_cache.h"
#include "sim/simulation.h"
#include "ui/ui_types.h"

#include <stdint.h>
#include <stdio.h>

static int finish_generation(void) {
    int guard = 0;
    while (!decision_snapshot_cache_service_slice() && guard++ < MAX_CIVS + 4) {}
    return guard <= MAX_CIVS + 4;
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
    ok &= case_pause_switch_redraw(summary);
    ok &= case_identity_restart_death_reuse(summary);
    ok &= case_reset_load_first_generation(summary);
    return ok;
}
