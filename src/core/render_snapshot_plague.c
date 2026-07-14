#include "core/render_snapshot_plague.h"

#include "core/render_snapshot_cache.h"

#include <string.h>

int render_snapshot_copy_plague(RenderSnapshot *snapshot, int key) {
    int i;
    int complete = 1;
    int active_known = 1;
    int active = 0;
    if (!render_snapshot_cache_plague_summary(key, &snapshot->plague_state,
                                              &snapshot->plague_metrics,
                                              &snapshot->plague_names)) {
        complete = 0;
        if (snapshot->revision == 0) {
            memset(&snapshot->plague_state, 0, sizeof(snapshot->plague_state));
            memset(&snapshot->plague_metrics, 0, sizeof(snapshot->plague_metrics));
            memset(&snapshot->plague_names, 0, sizeof(snapshot->plague_names));
            render_snapshot_cache_note_plague_fallback();
        }
    }
    if (!render_snapshot_cache_plague_impact(key, &snapshot->plague_impact)) {
        complete = 0;
        if (snapshot->revision == 0) {
            memset(&snapshot->plague_impact, 0, sizeof(snapshot->plague_impact));
            render_snapshot_cache_note_plague_fallback();
        }
    }
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

int render_snapshot_plague_effective_probability(int bucket) {
    const RenderSnapshot *snapshot = render_snapshot_acquire();
    int value = -1;
    if (snapshot) {
        const PlagueProbabilityDistribution *probabilities =
            &snapshot->plague_state.effective_probabilities;
        if (bucket == PLAGUE_PROBABILITY_NO_PLAGUE) {
            value = probabilities->no_plague;
        } else if (bucket == PLAGUE_PROBABILITY_SMALL) {
            value = probabilities->small;
        } else if (bucket == PLAGUE_PROBABILITY_MEDIUM) {
            value = probabilities->medium;
        } else if (bucket == PLAGUE_PROBABILITY_LARGE) {
            value = probabilities->large;
        }
        render_snapshot_release(snapshot);
    }
    return value;
}

int render_snapshot_plague_pending_probabilities_valid(void) {
    const RenderSnapshot *snapshot = render_snapshot_acquire();
    int valid = snapshot ?
        snapshot->plague_state.pending_probabilities_valid : -1;
    if (snapshot) render_snapshot_release(snapshot);
    return valid;
}
